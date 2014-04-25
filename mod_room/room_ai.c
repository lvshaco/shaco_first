#include "sh.h"
#include "room.h"
#include "room_tplt.h"
#include "room_game.h"
#include "room_genmap.h"
#include "msg_client.h"
#include <math.h>

#define AI_S_MOVE 0
#define AI_S_FOCUS 1

struct ai_target {
    uint32_t id;
    uint16_t w;
    uint16_t h;
    uint16_t block;
};

struct ai_brain {
    int level;
    int status;
    uint64_t status_intv;
    uint64_t status_time;
    uint64_t last_execute_time;
    float accu_depth; // 累积的距离
    int dir;
    float speed;
    struct ai_target target;
    struct ai_target target2;
    uint64_t press_time;
    int tick;
};

static int 
calc_buff_value(struct player *m) {
    int value = 0;
    struct sh_array *a = &m->total_effect;
    struct buff_effect *e;
    int i;
    for (i=0; i<a->nelem; ++i) {
        e = sh_array_get(a, i);
        if (e->id > 0) {
            value += e->effects[0].value;
        }
    }
    return (value >= -100) ? value : -100;
}

static inline float
fall_speed_base(struct player *m) {
    return m->base.charfallspeed * 0.75;
}

static inline float
fall_speed_standard(struct player *m) {
    return fall_speed_base(m)*pow(m->brain->level, 0.4)*0.17;
}

static inline float
oxygen_percent(struct player *m) {
    return m->detail.attri.oxygen / (float)m->base.oxygen;
}

static int
lookup_target(struct room *self, struct room_game *ro, struct player *m, 
        int type, struct ai_target *target) {
    struct genmap *map = ro->map;
    int depth = m->depth + 2; 
    if (depth <= 0 || depth > map->height) {
        return 1;
    }
    struct genmap_cell *cell;
    struct item_tplt *item;
    int i, start = (depth-1)*map->width;
    for (i=0; i<map->width; ++i) {
        cell = &map->cells[start+i];
        if (cell->cellid == 0 && cell->itemid > 0) {
            item = room_tplt_find_item(self, cell->itemid);
            if (item &&
                item->type == type) {
                target->id = item->id;
                target->h = depth-1;
                target->w = i;
                target->block = cell->block;
                return 0;
            }
        }
    }
    return 1;
}

static inline bool
rand_rate(int level, int rateup, int levelup, int ratedown, int leveldown) {
    float rate = (ratedown + 
                  (level - leveldown) * 
                  (float)(rateup-ratedown) / 
                  (float)(levelup-leveldown)) / 
        (float)(rand()%100);
    return rate >= 1;
}

static inline struct ai_target *
target_current(struct ai_brain *brain) {
    if (brain->target.id > 0) {
        return &brain->target;
    }
    if (brain->target2.id > 0) {
        return &brain->target2;
    }
    return NULL;
}

static inline int
target_depth(struct ai_target *target) {
    return target->h+1;
}

static int
down_block_count(struct room_game *ro, int depth) {
    struct genmap *map = ro->map;
    int h = depth;
    if (h < 0 || h >= map->height) {
        return 0;
    }
    int i, n=0;
    for (i=h*map->width; i<(h+1)*map->width; ++i) {
        if (CELL_IS_SHI(map->cells[i].cellid)) {
            n++;
        }
    }
    return n;
}

static inline uint64_t
wander_time(int level, int block) {
    return block * (1 - level/40.f);
}

static int
ai_dir(struct player *m) {
    struct ai_brain *brain = m->brain;
    struct ai_target *target = target_current(brain);
    if (target) {
        if (target_depth(target) > m->depth) {
            return 1;
        } else {
            return rand()%3 - 1;
        }
    } else {
        if (rand_rate(brain->level, 10, 1, 0, 10)) {
            return rand()%2 - 1;
        } else {
            return 1;
        }
    }
}

static float
ai_speed(struct room_game *ro, struct player *m) {
    struct ai_brain *brain = m->brain;
    if (brain->dir < 1)
        return 1;
    float speed;
    struct player *front = room_member_front(ro, m);
    if (front) {
        if (front->speed_new > front->speed_old) {
            speed = fall_speed_standard(m);
        } else {
            speed = fall_speed_base(m);
        }
    } else {
        speed = fall_speed_standard(m);
    }
    int buff_value = calc_buff_value(m);
    int down_block = down_block_count(ro, m->depth);
    int d = MAP_DEPTH(m->depth);
    int ntype = MAP_NTYPE(ro->map, d);
    down_block = down_block * 5 - pow(5-ntype, 4.5);
    speed *= (1+buff_value/100.f - down_block/100.f);
    return speed > 0 ? speed : 0;
}

static void
ai_update_move(struct module *s, struct player *m, float elapsed) {
    struct ai_brain *brain = m->brain;
    sh_trace("AI %u update move (%d, %f)", UID(m), brain->dir, brain->speed);
    if (brain->dir == 0) {
        return;
    }
    float dist = brain->dir * brain->speed * elapsed;
    brain->accu_depth += dist;
    int depth = brain->accu_depth;
    if (abs(depth) >= 1) {
        brain->accu_depth -= depth;
        depth += m->depth;
        if (depth < 0)
            depth = 0;
        UM_DEFFIX(UM_GAMESYNC, sync);
        sync->charid = m->detail.charid;
        sync->depth = depth;
        game_player_main(s, m, sync, sizeof(*sync));
        sh_trace("AI %u update depth (%d, cur %d)", UID(m), depth, m->depth);
    } 
}

static inline void
ai_focus_target(struct player *m, const struct ai_target *target) {
    struct ai_brain *brain = m->brain;
    brain->status = AI_S_FOCUS;
    brain->status_time = sh_timer_now();
    brain->status_intv = wander_time(brain->level, target->block); 
    sh_trace("AI %u switch to focus (%u in %u,%u b %u)", UID(m), 
            target->id, 
            target->w, 
            target->h, 
            target->block);
}

static inline bool
ai_can_pick(struct player *m) {
    struct ai_brain *brain = m->brain;
    if (brain->target.id > 0) {
        int rate = 110 - (m->depth - target_depth(&brain->target)) / 0.4;
        sh_trace("AI %u pick target1 rate %d", UID(m), rate);
        if (rate >= 100 ||
            rand()%100 <= rate) {
            return true;
        }
    } else if (brain->target2.id > 0) {
        sh_trace("AI %u pick target2 rate 100", UID(m));
        return true;
    }
    return false;

}
static void
ai_pick_target(struct module *s, struct room_game *ro, struct player *m) {
    struct ai_brain *brain = m->brain;
    struct ai_target *target = target_current(brain);
    if (target) {
        sh_trace("AI %u rand pick target (%u in %u,%u)", UID(m), 
            brain->target.id, 
            brain->target.w, 
            brain->target.h);
        if (ai_can_pick(m)) {
            sh_trace("AI %u pick target %u ok", UID(m), target->id);
            UM_DEFFIX(UM_USEITEM, use);
            use->itemid = target->id;
            game_player_main(s, m, use, sizeof(*use));
        }
        // clear
        struct genmap_cell *cell = GENMAP_CELL(ro->map, target->w, target->h);
        cell->itemid = 0;
        target->id = 0;
    }
    if (brain->target2.id > 0) {
        ai_focus_target(m, &brain->target2);
    } else {
        brain->status = AI_S_MOVE;
        brain->status_time = 0;
        brain->status_intv = 0; 
        sh_trace("AI %u switch to move", UID(m));
    }
}

static int
ai_lookup_oxygen(struct room *self, struct room_game *ro, struct player *m) {
    struct ai_brain *brain = m->brain;
    struct ai_target target;
    if (!lookup_target(self, ro, m, ITEM_T_OXYGEN, &target)) {
        int height = ro->map->height;
        float limit_per = 0.3 + 0.2*(1.2 - min(1.2, brain->level/7.0)) + 
            0.5 * ((height - m->depth)/height);
        float oxygen_per = oxygen_percent(m);
        sh_trace("AI %u lookup oxygen (%u in %u,%u b %u) oxygen_per %f limit_per %f", 
                UID(m), 
                target.id,
                target.w,
                target.h,
                target.block,
                oxygen_per,
                limit_per);
        if (oxygen_per <= limit_per) {
            brain->target = target;  
            return 0;
        }
    }
    return 1;
}

static int
ai_lookup_item(struct room *self, struct room_game *ro, struct player *m) {
    struct ai_brain *brain = m->brain;
    struct ai_target target;
    if (!lookup_target(self, ro, m, ITEM_T_FIGHT, &target)) {
        sh_trace("AI %u lookup item (%u in %u,%u b %u)",
                UID(m), 
                target.id,
                target.w,
                target.h,
                target.block);
        if (rand_rate(brain->level, 100, 6, 50, 1)) {
            brain->target = target; 
            return 0;
        }
    }
    return 1;
}

static void
ai_press(struct module *s, struct player *m, uint64_t now, int press_time) {
    struct ai_brain *brain = m->brain;
    brain->press_time = now;
    if (brain->status == AI_S_FOCUS) {
        brain->status_intv += press_time;
    }
    UM_DEFFIX(UM_ROLEPRESS, press);
    press->charid = m->detail.charid;
    game_player_main(s, m, press, sizeof(*press));
    sh_trace("AI %u press begin", UID(m));
}

void
ai_main(struct module *s, struct room_game *ro, struct player *m) {
    struct room *self = MODULE_SELF;
    struct ai_brain *brain = m->brain;
    
    uint64_t now = sh_timer_now();
    if (brain->last_execute_time == 0 ||
        brain->last_execute_time > now) {
        brain->last_execute_time = now;
        brain->dir = 0;
        brain->speed = ai_speed(ro, m);
        return;
    } 
    float elapsed = now - brain->last_execute_time;
    if (elapsed == 0) {
        return;
    }
    elapsed /= 1000;
    sh_trace("AI %u level %d elapsed %f", UID(m), brain->level, elapsed);
    int buff_value = calc_buff_value(m);

    bool is_trans = m->depth % 100 > 94;
    if (is_trans) { 
        float trans_speed = fall_speed_base(m) * 1.33 * (1+buff_value/100.f);
        brain->dir = 1;
        brain->speed = trans_speed;
        sh_trace("AI %u [trans] speed %f depth %d", UID(m), brain->speed, m->depth);
        ai_update_move(s, m, elapsed);
        goto ai_end;
    }

    if (brain->target.id == 0 && brain->target2.id == 0) {
        if (!ai_lookup_oxygen(self, ro, m) ||
            !ai_lookup_item(self, ro, m)) {
            ai_focus_target(m, &brain->target);
        } 
    }
    if (brain->target2.id == 0) {
        if (!lookup_target(self, ro, m, ITEM_T_BAO, &brain->target2)) {
            if (brain->target.id == 0) {
                ai_focus_target(m, &brain->target2);
            }
        }
    }

    if (brain->tick % 10 == 0) { 
        int press_time = m->detail.attri.rebirthtime;
        if (brain->press_time == 0) {
            sh_trace("AI %u press rand", UID(m));
            if (rand_rate(brain->level, 20, 1, 0, 8)) { 
                ai_press(s, m, now, press_time);
                goto ai_end;
            }
        } else { 
            if (press_time <= (int)(now - brain->press_time)) {
                brain->press_time = 0;
                sh_trace("AI %u press end", UID(m));
            }
            goto ai_end;
        } 
    }
    
    if (true/*brain->tick % 2 == 0*/) {
        brain->dir = ai_dir(m);
        brain->speed = ai_speed(ro, m);
    }
    ai_update_move(s, m, elapsed);

    if (brain->status == AI_S_FOCUS) {
        if (now - brain->status_time >= brain->status_intv) {
            ai_pick_target(s, ro, m);
        }
    }
ai_end:
    brain->last_execute_time = now;
    brain->tick++;
}

void ai_init(struct player *m, int level) {
    struct ai_brain *brain = malloc(sizeof(*brain));
    memset(brain, 0, sizeof(*brain));
    brain->level = level;
    m->brain = brain;
}

void ai_fini(struct player *m) {
    if (m->brain) {
        free(m->brain);
        m->brain = NULL;
    }
}
