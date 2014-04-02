#include "room.h"
#include "room_ai.h"
#include "room_item.h"
#include "room_buff.h"
#include "room_tplt.h"
#include "room_genmap.h"
#include "room_fight.h"
#include "room_dump.h"
#include "msg_client.h"
#include "msg_server.h"
#include "sh.h"
#include <string.h>
#include <math.h>

#define ROOM_LOAD_TIMELEAST 5
#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 3000
#define DESTROY_TIMEOUT 500

// refresh data type, binary bit
#define REFRESH_SPEED 1 
#define REFRESH_ATTRI 2

#define F_NO     0
#define F_CLIENT 1
#define F_OTHER  2
#define F_AWARD  4
#define F_OVER   8

static inline void
set_effect_state(struct player *m, int state) {
    m->detail.attri.effect_states |= 1<<state;
    m->refresh_flag |= REFRESH_ATTRI;
}

static inline void
clr_effect_state(struct player *m, int state) {
    m->detail.attri.effect_states &= ~(1<<state);
    m->refresh_flag |= REFRESH_ATTRI;
}

static inline bool
has_effect_state(struct player *m, int state) {
    return (m->detail.attri.effect_states & (1<<state)) != 0;
}

static inline uint64_t 
room_game_time(struct room_game *ro) {
    uint64_t gametime = sh_timer_now() - ro->starttime;
    if (gametime < 1000)
        gametime = 1000;
    return gametime;
}

static void
member_free(struct room *self, struct player* m) {
    sh_array_fini(&m->total_delay);
    sh_array_fini(&m->total_effect);
    ai_fini(m);
    if (m->online) {
        sh_hash_remove(&self->players, UID(m));
        m->online = false;     
    }
}

static void
free_room_game(struct room *self, struct room_game *ro) {
    if (ro->map) {
        genmap_free(ro->map);
        ro->map = NULL;
    }
    int i;
    for (i=0; i<ro->np; ++i) {
        member_free(self, &ro->p[i]);
    }
    room_item_fini(ro);
    free(ro);
}

static inline bool
elapsed(uint64_t t, uint64_t elapse) {
    uint64_t now = sh_timer_now();
    return now > t && (now - t >= elapse);
}

static inline void
notify_exit_room(struct module *s, int source, uint32_t uid) {
    sh_trace("Room notify %u exit room", uid);
    UM_DEFFIX(UM_EXITROOM, exit);
    exit->uid = uid;
    sh_module_send(MODULE_ID, source, MT_UM, exit, sizeof(*exit));
}

static inline void
notify_play_fail(struct module *s, int source, uint32_t uid, int err) {
    sh_trace("Room notify %u play fail %d", uid, err);
    UM_DEFWRAP(UM_CLIENT, cl, UM_PLAYFAIL, pf);
    cl->uid = uid;
    pf->err = err;
    sh_module_send(MODULE_ID, source, MT_UM, cl, sizeof(*cl)+sizeof(*pf));
}

static inline void
notify_create_room_game_result(struct module *s, int dest_handle, uint32_t roomid, int err) {
    sh_trace("Room %u notify match create result %d", roomid, err);
    UM_DEFFIX(UM_CREATEROOMRES, result);
    result->id = roomid;
    result->err = err;
    sh_module_send(MODULE_ID, dest_handle, MT_UM, result, sizeof(*result));
}

static inline void
notify_join_room_game_result(struct module *s, int source, uint32_t id, uint32_t uid, int err) {
    sh_trace("Room %u notify join result %d", id, err);
    UM_DEFFIX(UM_JOINROOMRES, result);
    result->id = id;
    result->uid = uid;
    result->err = err;
    sh_module_send(MODULE_ID, source, MT_UM, result, sizeof(*result));
}

static inline void
notify_award(struct module *s, struct room_game *ro, struct player *m, 
        const struct memberaward *award) {
    sh_trace("Room %u notify %u award", ro->id, UID(m));
    UM_DEFWRAP(UM_HALL, ha, UM_GAMEAWARD, ga);
    ha->uid  = UID(m);
    ga->type = ro->type;
    ga->award = *award;
    sh_module_send(MODULE_ID, m->watchdog_source, MT_UM, ha, sizeof(*ha)+sizeof(*ga));
}

static void
multicast_msg(struct module *s, struct room_game* ro, const void *msg, int sz, uint32_t except) {
    UM_DEFWRAP2(UM_CLIENT, cl, sz); 
    memcpy(cl->wrap, msg, sz);
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (UID(m) != except &&
            is_online(m) &&
            is_player(m)) {
            cl->uid = UID(m);
            sh_module_send(MODULE_ID, m->watchdog_source, MT_UM, cl, sizeof(*cl)+sz);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// room_game logic

static inline void
build_brief_from_detail(struct tmemberdetail *detail, struct tmemberbrief *brief) {
    brief->accid = detail->accid;
    brief->charid = detail->charid;
    memcpy(brief->name, detail->name, CHAR_NAME_MAX);
    brief->level = detail->level;
    brief->role = detail->role;
    brief->state = detail->state;
    brief->oxygen = detail->attri.oxygen;
    brief->body = detail->attri.body;
    brief->quick = detail->attri.quick;
}

static inline void
fill_brief_into_detail(struct tmemberbrief *brief, struct tmemberdetail *detail) {
    detail->accid = brief->accid;
    detail->charid = brief->charid;
    memcpy(detail->name, brief->name, CHAR_NAME_MAX);
    detail->level = brief->level;
    detail->role = brief->role;
    detail->state = brief->state;
    detail->attri.oxygen = brief->oxygen;
    detail->attri.body = brief->body;
    detail->attri.quick = brief->quick;
}

static struct player *
member_get(struct room_game *ro, uint32_t accid) {
    int i;
    for (i=0; i<ro->np; ++i) {
        if (ro->p[i].detail.accid == accid)
            return &ro->p[i];
    }
    return NULL;
}

static int
find_free_member(struct room_game *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        if (is_offline(&ro->p[i])) {
            return i;
        }
    }
    if (ro->np < ro->maxp) {
        return ro->np;
    }
    return -1;
}

static inline void
member_place(struct room_game *ro, struct player *m, struct match_member *mm, uint8_t index) {
    memset(m, 0, sizeof(*m)); 
    m->index = index;
    if (ro->type == ROOM_TYPE_DASHI) {
        m->team = index; 
    } 
    m->is_robot = mm->is_robot;
    fill_brief_into_detail(&mm->brief, &m->detail);
}

static void
build_award_normal(struct room_game *ro, uint64_t gametime, struct player *m, 
        struct memberaward *award) {
    struct char_attribute* a = &m->detail.attri; 
    int score_depth = pow(m->depth, 0.5) * 100;
    int score_speed = pow(m->depth/(gametime*0.001), 1.2) * 766;
    int score_oxygen = pow(m->noxygenitem, 1.2) * 20;
    int score_item = (m->nitem + m->ntrap) * 20;
    int score_bao = pow(m->nbao, 1.5) * 100;
   
    float coin_profit = 1+a->coin_profit;
    float score_profit = 1+a->score_profit;

    int coin = (score_depth + score_speed + score_oxygen) * 0.1 * coin_profit;
    int exp = (m->depth * 0.2 + m->nbao);

    int score_normal = 
        (score_depth + score_speed + score_oxygen + score_item + score_bao) * 
        score_profit * 10;
   
    int score_dashi = 0;
    int take_state = 20;

    award->take_state = take_state; 
    award->exp = exp;
    award->coin = coin;
    award->score_normal = score_normal;
    award->score_dashi = score_dashi;
    award->luck_factor = m->luck_factor;
}

struct award_input {
    int score_agv;
    int max_item;
    int max_trap;
    int max_bedamage;
};

static void
build_award_dashi(struct room_game *ro, uint64_t gametime, const struct award_input *in, 
        struct player *m, int rank, struct memberaward *award) {
    struct char_attribute* a = &m->detail.attri; 
    int score_depth = pow(m->depth, 0.5) * 100;
    int score_speed = pow(m->depth/(gametime*0.001), 1.2) * 766;
    int score_oxygen = pow(m->noxygenitem, 1.2) * 20;
    int score_item = (m->nitem + m->ntrap) * 20;
    int score_bao = pow(m->nbao, 1.5) * 100;
  
    float coin_profit = 1+a->coin_profit;
    float score_profit = 1+a->score_profit;
    if (rank == 0) {
        coin_profit += a->wincoin_profit+0.05;
        score_profit += a->winscore_profit + 0.05;
    }
    int coin = (score_depth + score_speed + score_oxygen) * 0.1 * coin_profit;
    int exp = (m->depth * 0.2 + m->nbao);

    int score_normal = 
        (score_depth + score_speed + score_oxygen + score_item + score_bao) * 
        score_profit * 10;

    // score_dashi
    const int score_line1 = 1500;
    const int score_line2 = 5000;
    int score_last = m->detail.score_dashi;
    int score_dashi;
    int score_diff = (score_last - in->score_agv) * 2;
    int score_cut = 0;
    int score_ext = 0;
    if (score_last >= score_line1) {
        float t = min(score_line2-score_line1, 
                      max(500, (score_last-score_line1)))/500.f;
        score_cut = (int)t;
        if (score_cut < t)
            score_cut += 1;
    }
    if (m->nitem < in->max_item)
        score_ext += 32 * 0.05;
    if (m->ntrap >= in->max_trap)
        score_ext += 32 * 0.03;
    if (m->nbedamage >= in->max_bedamage)
        score_ext += 32 * 0.05;
    if (rank == 0) {
        score_dashi = max(10, min(62, 32 - score_diff * 0.06 - score_cut)) + score_ext;
    } else if (score_last >= 1500) {
        score_dashi = max(-66,min(-13,-26 - score_diff * 0.12 - score_cut)) + score_ext;
    } else {
        score_dashi = 0;
    }
    
    int take_state = (rank == 0) ? 10 : 20;

    award->take_state = take_state; 
    award->exp = exp;
    award->coin = coin;
    award->score_normal = score_normal;
    award->score_dashi = score_dashi;
    award->luck_factor = m->luck_factor;
}

static void
build_award_input(struct room_game *ro, bool death, struct player **p, int np, 
        struct award_input *in) {
    assert(np > 0);
    int i;
    int score_sum = 0;
    memset(in, 0, sizeof(*in));
    for (i=0; i<np; ++i) {
        struct player *m = p[i];
        if (in->max_item < m->nitem)
            in->max_item = m->nitem;      
        if (in->max_trap < m->ntrap)
            in->max_trap = m->ntrap;
        if (in->max_bedamage < m->nbedamage)
            in->max_bedamage = m->nbedamage;

        score_sum += m->detail.score_dashi;
    } 
    in->score_agv = score_sum/np;
}

static inline void
build_stat(struct room_game *ro, struct player *m, struct memberaward *a, struct tmemberstat *st) {
    st->charid = m->detail.charid;
    st->depth = m->depth;
    st->noxygenitem = m->noxygenitem;
    st->nitem = m->nitem;
    st->nbao = m->nbao;
    st->exp = a->exp;
    st->coin = a->coin;
    if (ro->type == ROOM_TYPE_NORMAL) {
        st->score = a->score_normal;
    } else {
        st->score = a->score_dashi;
    }
}

static int 
rankcmp_oxygen(const void* p1, const void* p2) {
    const struct player* m1 = *(const struct player**)p1; 
    const struct player* m2 = *(const struct player**)p2;
    return m2->detail.attri.oxygen - m1->detail.attri.oxygen;
}

static int 
rankcmp_depth(const void* p1, const void* p2) {
    const struct player* m1 = *(const struct player**)p1; 
    const struct player* m2 = *(const struct player**)p2;
    return m2->depth - m1->depth;
}

static void
member_over(struct module *s, struct room_game *ro, struct player *m, int flag) {
    struct room *self = MODULE_SELF;
    sh_trace("Room %u member %u over", ro->id, UID(m)); 
    if (!is_online(m)) {
        return;
    }
    uint64_t gametime = room_game_time(ro);
    // award to hall
    struct memberaward a;
    if ((flag & F_AWARD) && is_player(m)) {
        build_award_normal(ro, gametime, m, &a);
        notify_award(s, ro, m, &a);
    } else {
        memset(&a, 0, sizeof(a));
    }
    // switch to hall
    { 
        notify_exit_room(s, m->watchdog_source, UID(m));
    }

    struct tmemberstat stat;
    build_stat(ro, m, &a, &stat);
    
    // multicast unjoin
    if ((flag & F_OTHER) && 
        (ro->status == ROOMS_START)) {
        UM_DEFFIX(UM_GAMEUNJOIN, unjoin);
        unjoin->stat = stat;
        multicast_msg(s, ro, unjoin, sizeof(*unjoin), UID(m));
    }
    // overinfo to client
    if ((flag & F_CLIENT) && is_player(m)) {
        UM_DEFVAR(UM_CLIENT, cg);
        cg->uid = UID(m);
        UD_CAST(UM_GAMEOVER, go, cg->wrap);
        go->type = ro->type;
        go->nmember = 1;
        go->stats[0] = stat;
        int gosz = UM_GAMEOVER_size(go);
        sh_module_send(MODULE_ID, m->watchdog_source, MT_UM, cg, sizeof(*cg)+gosz);
    }
   
    if (m->online) {
        sh_hash_remove(&self->players, UID(m));
        m->online = false;
    }
}

static void
game_over(struct module *s, struct room_game* ro, bool death) {
    struct room *self = MODULE_SELF;

    if (ro->status == ROOMS_OVER) {
        return;
    }
    sh_trace("Room %u over, by death? %d", ro->id, death); 
    int i;
    int np = ro->np;
    assert(np <= MEMBER_MAX);
    uint64_t gametime = room_game_time(ro);
    
    struct player* m;
    struct player* sortm[MEMBER_MAX];
    struct memberaward *a;
    struct memberaward awards[MEMBER_MAX];

    // rank sort
    for (i=0; i<np; ++i) {
        sortm[i] = &ro->p[i];
    }
    if (death) {
        qsort(sortm, np, sizeof(sortm[0]), rankcmp_oxygen);
    } else {
        qsort(sortm, np, sizeof(sortm[0]), rankcmp_depth);
    }
    
    // award to hall
    struct award_input in;
    build_award_input(ro, death, sortm, np, &in);
    for (i=0; i<np; ++i) {
        m = sortm[i];
        a = &awards[i];
        build_award_dashi(ro, gametime, &in, m, i, a);
        if (is_online(m) && is_player(m)) {
            notify_award(s, ro, m, a);
        }
    }

    // switch to hall
    for (i=0; i<np; ++i) {
        m = sortm[i];
        if (is_online(m)) {
            notify_exit_room(s, m->watchdog_source, UID(m));
        }
    }

    // overinfo to client
    struct tmemberstat *st;
    UM_DEFVAR(UM_CLIENT, cg);
    UD_CAST(UM_GAMEOVER, go, cg->wrap);
    go->type = ro->type;
    go->nmember = np;
    for (i=0; i<np; ++i) {
        m = sortm[i];
        st = &go->stats[i];
        a = &awards[i];
        st->charid = m->detail.charid;
        st->depth = m->depth;
        st->noxygenitem = m->noxygenitem;
        st->nitem = m->nitem;
        st->nbao = m->nbao;
        st->exp = a->exp;
        st->coin = a->coin;
        if (ro->type == ROOM_TYPE_NORMAL) {
            st->score = a->score_normal;
        } else {
            st->score = a->score_dashi;
        }
    }  
    int gosz = UM_GAMEOVER_size(go);
    for (i=0; i<np; ++i) {
        m = &ro->p[i];
        cg->uid = UID(m);
        if (is_online(m) && is_player(m)) {
            sh_module_send(MODULE_ID, m->watchdog_source, MT_UM, cg, sizeof(*cg)+gosz);
        }
    }

    // remove member
    for (i=0; i<np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            sh_hash_remove(&self->players, UID(m));
            m->online = false;
        }
    }

    // over room_game
    ro->status = ROOMS_OVER;
    ro->statustime = sh_timer_now();
}

static void
room_game_create(struct module *s, int source, struct UM_CREATEROOM *create) {
    sh_trace("Room %u recevie create from mapid %u", create->id, create->mapid);
    struct room* self = MODULE_SELF;
    struct room_game *ro = sh_hash_find(&self->room_games, create->id);
    if (ro) {
        notify_create_room_game_result(s, source, create->id, SERR_ROOMIDCONFLICT);
        return;
    }
    const struct map_tplt *mapt = room_tplt_find_map(self, create->mapid); 
    if (mapt == NULL) {
        notify_create_room_game_result(s, source, create->id, SERR_CRENOTPLT);
        return;
    }
    struct roommap *mapdata = room_mapdata_find(self, mapt->id);
    if (mapdata == NULL) {
        notify_create_room_game_result(s, source, create->id, SERR_CRENOMAP);
        return;
    }
    uint32_t map_randseed = self->map_randseed;
    struct genmap* mymap = genmap_create(mapt, mapdata, map_randseed);
    if (mymap == NULL) {
        notify_create_room_game_result(s, source, create->id, SERR_CREMAP);
        return;
    }

    ro = malloc(sizeof(*ro));
    ro->id = create->id;
    ro->type = create->type;
    ro->map = mymap;
    ro->status = ROOMS_CREATE;
    ro->statustime = sh_timer_now();
    ro->starttime = sh_timer_now(); 
    ro->gattri.randseed = map_randseed;
    ro->gattri.mapid = create->mapid;
    ground_attri_build(mapt->difficulty, &ro->gattri); 
    int i;
    ro->maxp = min(create->max_member, MEMBER_MAX);
    ro->np = create->nmember; 
    for (i=0; i<create->nmember; ++i) {
        member_place(ro, &ro->p[i], &create->members[i], i);
    }
    room_item_init(self, ro, mapt);
    assert(!sh_hash_insert(&self->room_games, ro->id, ro));
    notify_create_room_game_result(s, source, create->id, SERR_OK);
    self->map_randseed++; 
    sh_trace("Room %u mapid %u create ok", create->id, create->mapid);
}

static void
room_game_join(struct module *s, int source, uint32_t id, struct match_member *mm) {
    struct room* self = MODULE_SELF;
    
    uint32_t uid = mm->brief.accid;
    sh_trace("Room %u recevie join %u", id, uid);
    
    struct room_game *ro = sh_hash_find(&self->room_games, id);
    if (ro == NULL) {
        notify_join_room_game_result(s, source, id, uid, SERR_NOROOMJOIN);
        return;
    }
    struct player *m = member_get(ro, uid);
    if (m) {
        notify_join_room_game_result(s, source, id, uid, SERR_OK);
        return;
    }
    int index = find_free_member(ro);
    if (index == -1) {
        notify_join_room_game_result(s, source, id, uid, SERR_ROOMFULL);
        return;
    }
    member_place(ro, &ro->p[index], mm, index);
    if (index >= ro->np) {
        ro->np++; 
    }
    notify_join_room_game_result(s, source, id, uid, SERR_OK);
}

static void
room_game_destroy(struct module *s, struct room_game *ro) {
    sh_trace("Room %u destroy", ro->id);
    struct room *self = MODULE_SELF;
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (is_online(m)) {
            sh_trace("Room %u destroy, then logout member %u", ro->id, UID(m));
            member_over(s, ro, m, F_OVER);
        }
    }
    sh_hash_remove(&self->room_games, ro->id);
    free_room_game(self, ro);
}

static void
room_game_destroy_byid(struct module *s, uint32_t roomid) {
    sh_trace("Room %u destroy by id", roomid);
    struct room *self = MODULE_SELF;
    struct room_game *ro = sh_hash_find(&self->room_games, roomid);
    if (ro) {
        room_game_destroy(s, ro);
    }
}

static void
room_game_enter(struct module *s, struct room_game* ro) {
    sh_trace("Room %u multi enter", ro->id); 
    ro->status = ROOMS_ENTER;
    ro->statustime = sh_timer_now();
    UM_DEFFIX(UM_GAMEENTER, enter);
    multicast_msg(s, ro, enter, sizeof(*enter), 0);
}

static void
room_game_start(struct module *s, struct room_game* ro) {
    sh_trace("Room %u multi start", ro->id); 
    ro->status = ROOMS_START; 
    ro->starttime = sh_timer_now();
    ro->statustime = ro->starttime;
    UM_DEFFIX(UM_GAMESTART, start);
    multicast_msg(s, ro, start, sizeof(*start), 0);
}

static bool
is_total_loadok(struct room_game *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        if (!ro->p[i].loadok)
            return false;
    }
    return true;
}

static bool
check_enter_room_game(struct module *s, struct room_game *ro) {
    if (ro->status != ROOMS_CREATE) {
        return false;
    }
    if (!elapsed(ro->statustime, ENTER_TIMELEAST)) {
        return false;
    }
    if (is_total_loadok(ro)) {
        room_game_enter(s, ro);
        return true;
    } 
    if (!elapsed(ro->statustime, ENTER_TIMEOUT)) {
        return false;
    }
    if (room_online_nplayer(ro) > 0) {
        room_game_enter(s, ro);
        return true;
    } else {
        // maybe no player login, or 
        // logined but logout now
        room_game_destroy(s, ro);
        return false;
    }
}

static bool
check_start_room_game(struct module *s, struct room_game *ro) {
    if (ro->status != ROOMS_ENTER) {
        return false;
    }
    if (elapsed(ro->statustime, START_TIMEOUT)) {
        room_game_start(s, ro);
        return true;
    }
    return false;
}

static struct player *
someone_death(struct room_game *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (is_online(m)) {
            if (m->detail.attri.oxygen == 0)
                return m;
        }
    }
    return false;
}

static bool
check_over_room_game(struct module *s, struct room_game *ro) {
    if (ro->status != ROOMS_START) {
        return false;
    }
    struct player *m = someone_death(ro);
    if (m) {
        if (ro->type == ROOM_TYPE_NORMAL) {
            member_over(s, ro, m, F_CLIENT|F_OTHER|F_AWARD);
        } else {
            game_over(s, ro, true);
        }
    }
    int n = room_online_nplayer(ro);
    if (n == 0) {
        room_game_destroy(s, ro);
    }
    return true;
}

static bool
check_destroy_room_game(struct module *s, struct room_game *ro) {
    if (ro->status != ROOMS_OVER) {
        return false;
    }
    if (elapsed(ro->statustime, DESTROY_TIMEOUT)) {
        room_game_destroy(s, ro);
        return true;
    }
    return false;
}


//////////////////////////////////////////////////////////////////////
static void
notify_game_info(struct module *s, struct player *m) {
    struct room_game *ro = room_member_to_game(m);

    if (is_player(m)) {
        UM_DEFVAR(UM_CLIENT, cl); 
        UD_CAST(UM_GAMEINFO, gi, cl->wrap);
        cl->uid = UID(m);
        gi->load_least_time = ROOM_LOAD_TIMELEAST;
        gi->status = ro->status;
        gi->gattri = ro->gattri;
        
        int i, n=0;
        for (i=0; i<ro->np; ++i) {
            struct player *m = &ro->p[i];
            gi->members[i] = m->detail;
            n++;
        }
        gi->nmember = n;
        int sz = UM_GAMEINFO_size(gi);
        sh_module_send(MODULE_ID, m->watchdog_source, MT_UM, cl, sizeof(*cl) + sz);
    }
    UM_DEFFIX(UM_GAMEMEMBER, gm);
    gm->member = m->detail;
    multicast_msg(s, ro, gm, sizeof(*gm), UID(m));
} 

static void
on_refresh_attri(struct module *s, struct player* m, struct room_game* ro) {
    if (m->refresh_flag == 0)
        return;
    if (m->refresh_flag & REFRESH_SPEED) {
        role_attri_build(&ro->gattri, &m->detail.attri);
    }
    if (m->refresh_flag & REFRESH_ATTRI) {
        UM_DEFFIX(UM_ROLEINFO, ri);
        ri->detail = m->detail;
        multicast_msg(s, ro, ri, sizeof(*ri), 0);
    }
    m->refresh_flag = 0;
}

static float
do_effect(struct player* m, struct char_attribute* cattri, const struct char_attribute* base, 
        int32_t type, float value, bool isper) {
#define AMAX 10000000
#define CASE(T, R, B, V, isper, min, max, flag) \
    case T: { \
        float old = R; \
        (R) += (isper) ? (B)*(V)*0.001 : (V); \
        if (R < min) R = min; \
        else if (R > max) R = max; \
        float diff = R - old; \
        if (diff != 0) m->refresh_flag |= flag; \
        return diff; \
    }
    switch (type) {
    CASE(EFFECT_OXYGEN, cattri->oxygen, base->oxygen, value, isper, 0, base->oxygen, REFRESH_ATTRI);
    CASE(EFFECT_BODY, cattri->body, base->body, value, isper, 0, AMAX, REFRESH_ATTRI|REFRESH_SPEED);
    CASE(EFFECT_QUICK, cattri->quick, base->quick, value, isper, 0, AMAX, REFRESH_ATTRI|REFRESH_SPEED);
    //CASE(EFFECT_COIN_PROFIT, cattri->coin_profit, 1, value, isper, REFRESH_ATTRI);
    CASE(EFFECT_MOVE_SPEED, cattri->movespeedadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI|REFRESH_SPEED);
    CASE(EFFECT_FALL_SPEED, cattri->charfallspeedadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI|REFRESH_SPEED);
    CASE(EFFECT_ATTACK_DISTANCE, cattri->attack_distance, base->attack_distance, value, isper, 0, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_ATTACK_RANGE, cattri->attack_range, base->attack_range, value, isper, 0, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_ATTACK_POWER, cattri->attack_power, base->attack_power, value, isper, 0, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_LUCK, cattri->lucky, base->lucky, value, isper, 0, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_ATTACK_SPEED, cattri->attack_speed, base->attack_speed, value, isper, 0, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_DODGE_DISTANCE, cattri->dodgedistanceadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI|REFRESH_SPEED);  
    CASE(EFFECT_REBIRTH_TIME, cattri->rebirthtimeadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI|REFRESH_SPEED);
    CASE(EFFECT_JUMP_RANGE, cattri->jump_range, base->jump_range, value, isper, 0, AMAX, REFRESH_ATTRI); 
    CASE(EFFECT_SENCE_RANGE, cattri->sence_range, base->sence_range, value, isper, 0, AMAX, REFRESH_ATTRI);
    //CASE(EFFECT_WINCOIN_PROFIT, cattri->wincoin_profit, 1, value, isper, REFRESH_ATTRI);
    //CASE(EFFECT_EXP_PROFIT, cattri->exp_profit, 1, value, isper, REFRESH_ATTRI);
    CASE(EFFECT_ITEM_TIME, cattri->item_timeadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_ITEM_OXYGEN, cattri->item_oxygenadd, 1, value, isper, -1, AMAX, REFRESH_ATTRI);
    CASE(EFFECT_VIEW_RANGE, cattri->view_range, base->view_range, value, isper, 0, AMAX, REFRESH_ATTRI);
    //CASE(EFFECT_SCORE_PROFIT, cattri->score_profit, 1, value, isper, REFRESH_ATTRI);
    //CASE(EFFECT_WINSCORE_PROFIT, cattri->winscore_profit, 1, value, isper, REFRESH_ATTRI);
    case EFFECT_STATE: {
        int ivalue = value;
        if (ivalue > 0) {
            set_effect_state(m, ivalue);
        } else {
            clr_effect_state(m, -ivalue);
        }
        return ivalue;
    }
    default:
        return 0.0f;
    }
} 

static float
item_effectone(struct player* m, struct one_effect* effect) {
    if (effect->type > 0 && effect->value != 0) {
        return do_effect(m, &m->detail.attri, &m->base, 
                effect->type, effect->value, effect->isper);
    }
    return 0;
}

static void
item_effect_member(struct module *s, struct room_game *ro, struct player *m, 
        const struct item_tplt* item, int addtime) {
    struct one_effect tmp[BUFF_EFFECT];
    struct one_effect* effectptr = NULL;
    struct buff_effect *effect;

    if (item->time > 0) {
        effect = buff_effect_find(&m->total_effect, item->id);
        if (effect == NULL) {
            effect = buff_effect_add(&m->total_effect, item->id);
            assert(effect);
            effectptr = effect->effects;
        }
        effect->time = sh_timer_now() + item->time*1000 + addtime;
        sh_trace("insert time: %llu, to char %u", (unsigned long long)effect->time, m->detail.charid);
    } else {
        effectptr = tmp;
    }
    if (effectptr) {
#define FILL_EFFECT(n) \
        if (n <= BUFF_EFFECT) { \
            effectptr[n-1].type  = item->effect##n; \
            effectptr[n-1].isper = item->valuet##n; \
            effectptr[n-1].value = item->value##n;  \
        }
        FILL_EFFECT(1);
        FILL_EFFECT(2);
        FILL_EFFECT(3);

        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            effectptr[i].value = item_effectone(m, &effectptr[i]);
            effectptr[i].isper = false;
        }
        on_refresh_attri(s, m, ro);
        //room_dump_player(m);
    }
}

static inline int 
get_effect_members(struct room_game* ro, 
                   struct player* me, 
                   int target, 
                   struct player* ret[MEMBER_MAX]) {
    switch (target) {
    case ITEM_TARGET_SELF:
        ret[0] = me;
        return 1;
    case ITEM_TARGET_ENEMY: {
        struct player* m;
        int i, n=0;
        for (i=0; i<ro->np; ++i) {
            m = &ro->p[i];
            if (is_online(m) && 
                m->team != me->team) {
                ret[n++] = m;
            }
        }
        return n;
        }
    case ITEM_TARGET_FRIEND: {
        struct player* m;
        int i, n=0;
        for (i=0; i<ro->np; ++i) {
            m = &ro->p[i];
            if (is_online(m) && 
                m->team == me->team) {
                ret[n++] = m;
            }
        }
        return n;
        }
    case ITEM_TARGET_ALL: {
        struct player* m;
        int i, n=0;
        for (i=0; i<ro->np; ++i) {
            m = &ro->p[i];
            if (is_online(m)) {
                ret[n++] = m;
            }
        }
        return n;
        }
    default:
        return 0;
    }
}

static void
item_effect(struct module* s, struct room_game* ro, struct player* me, 
        const struct item_tplt* item, int addtime) {
    sh_trace("char %u, item effect %u, item time %d(s), add time %d", 
            me->detail.charid, item->id, item->time, addtime);

    struct player* tars[MEMBER_MAX];
    int ntar = get_effect_members(ro, me, item->target, tars);
    if (ntar <= 0) {
        return;
    }
    struct player* onetar;
    int i;
    for (i=0; i<ntar; ++i) {
        onetar = tars[i];
        item_effect_member(s, ro, onetar, item, addtime);
    }
}

static void
item_delay(struct room *self, struct player *m, const struct item_tplt *item, int delay_time) {
    sh_trace("char %u, use delay item %u", m->detail.charid, item->id);

    uint64_t effect_time = sh_timer_now() + delay_time;
    struct buff_delay* delay = buff_delay_find(&m->total_delay, item->id);
    if (delay == NULL) {
        delay = buff_delay_add(&m->total_delay, item->id);
        assert(delay);
        delay->first_time = effect_time;
    } 
    delay->last_time = effect_time;
}

static uint32_t
rand_baoitem(struct room *self, const struct item_tplt *item, const struct map_tplt *mapt) {
#define CASE_BAO(n) case n: \
    return mapt->baoitem ## n[sh_rand(&self->randseed)%mapt->nbaoitem ## n] \

    switch (item->subtype) {
    CASE_BAO(1);
    CASE_BAO(2);
    CASE_BAO(3);
    CASE_BAO(4);
    CASE_BAO(5);
    CASE_BAO(6);
    CASE_BAO(7);
    CASE_BAO(8);
    CASE_BAO(9);
    CASE_BAO(10);
    }
    return 0;
}

static inline const struct item_tplt*
rand_fightitem(struct room *self, const struct map_tplt *mapt) {
    uint32_t randid = mapt->fightitem[rand()%mapt->nfightitem];
    return room_tplt_find_item(self, randid);
}

static inline const struct item_tplt*
rand_trapitem(struct room *self, const struct map_tplt *mapt) {
    uint32_t randid = mapt->trapitem[rand()%mapt->ntrapitem];
    const struct item_tplt* item = room_tplt_find_item(self, randid);
    if (item == NULL) {
        sh_trace("not found rand item %u", randid);
    }
    return item;
}

static void
use_item(struct module *s, struct player *m, const struct UM_USEITEM *use) {
    struct room *self = MODULE_SELF;

    struct room_game *ro = room_member_to_game(m);

    const struct item_tplt* item = room_tplt_find_item(self, use->itemid);
    if (item == NULL) {
        sh_trace("Room %u not found use item: %u", ro->id, use->itemid);
        return;
    }
    const struct map_tplt* map = room_tplt_find_map(self, ro->gattri.mapid); 
    if (map == NULL) {
        return;
    }
    uint32_t init_itemid = item->id;
    switch (item->type) {
    case ITEM_T_OXYGEN:
        m->noxygenitem += 1;
        break;
    case ITEM_T_FIGHT:
        item = room_item_rand(self, ro, m, item);
        if (item == NULL) {
            return;
        }
        m->nitem += 1;
        break;
    case ITEM_T_TRAP:
        if (item->subtype == 0) {
            item = rand_trapitem(self, map);
            if (item == NULL) {
                return;
            }
        }
        break;
    case ITEM_T_BAO: {
        uint32_t baoid = rand_baoitem(self, item, map);
        if (baoid > 0) {
            m->nbao += 1;
        }
        }
        break;
    }

    struct player* tars[MEMBER_MAX];
    int ntar = get_effect_members(ro, m, item->target, tars);
    if (ntar <= 0) {
        return;
    }

    int delay = item->delay + item->uptime;
    if (delay > 0) {
        item_delay(self, m, item, delay); 
    } else {
        int i;
        for (i=0; i<ntar; ++i) {
            struct player *one = tars[i];
            item_effect_member(s, ro, one, item, 0);
        }
    }

    UM_DEFVAR(UM_ITEMEFFECT, ie);
    ie->spellid = m->detail.charid;
    ie->oriitem = init_itemid;
    ie->itemid = item->id;
    ie->ntarget = ntar;
    int i;
    for (i=0; i<ntar; ++i) {
        ie->targets[i] = UID(tars[i]);
    }
    multicast_msg(s, ro, ie, UM_ITEMEFFECT_size(ie), 0);
}

static inline int
reduce_oxygen(struct player* m, int oxygen) {
    if (oxygen > 0) {
        int old = m->detail.attri.oxygen;
        if (m->detail.attri.oxygen > oxygen)
            m->detail.attri.oxygen -= oxygen;
        else
            m->detail.attri.oxygen = 0;
        return old - m->detail.attri.oxygen;
    }
    return 0;
}

static void
loadok(struct module *s, struct player *m) {
    if (!m->loadok) {
        struct room_game *ro = room_member_to_game(m);
        m->loadok = true;
        sh_trace("Room %u player %u load ok", ro->id, UID(m));
        check_enter_room_game(s, ro);
    }
}

static struct player *
login(struct module *s, int source, uint32_t roomid, float luck_factor, 
        const struct tmemberdetail *detail) { 
    struct room *self = MODULE_SELF;
    struct player *m;
    uint32_t accid  = detail->accid;

    m = sh_hash_find(&self->players, accid);
    if (m) {
        sh_warning("Room member %u relogin, free old", accid);
        member_free(self, m); // just free old
    }
    struct room_game *ro = sh_hash_find(&self->room_games, roomid); 
    if (ro == NULL) {
        notify_exit_room(s, source, accid);
        notify_play_fail(s, source, accid, SERR_NOROOMLOGIN);
        return NULL; // someting wrong
    }
    m = member_get(ro, accid);
    if (m == NULL || m->online) {
        notify_exit_room(s, source, accid);
        notify_play_fail(s, source, accid, SERR_NOMEMBER);
        return NULL; // someting wrong
    }
    // free first
    member_free(self, m);

    // fill new data
    m->luck_factor = luck_factor;
    m->detail = *detail; 
    role_attri_build(&ro->gattri, &m->detail.attri);
    m->base = m->detail.attri;
    
    //room_dump_ground(&ro);
    //room_dump_player(m);
    
    sh_array_init(&m->total_delay, sizeof(struct buff_delay), 1);
    sh_array_init(&m->total_effect, sizeof(struct buff_effect), 1);

    m->logined = true; 
    m->watchdog_source = source;
    m->brain = NULL; 
    {
    assert(!sh_hash_insert(&self->players, accid, m));
    m->online = true;
    }
    return m;
}

static void
player_login(struct module *s, int source, const struct UM_LOGINROOM *lr) { 
    struct player *m = login(s, source, lr->roomid, lr->luck_factor, &lr->detail);
    if (m) {
        sh_trace("Room %u player %u login ok", lr->roomid, lr->detail.accid);
        m->brain = NULL;
        notify_game_info(s, m);
    } else {
        sh_trace("Room %u player %u login fail", lr->roomid, lr->detail.accid);
    }
}

static void
robot_login(struct module *s, int source, const struct UM_ROBOT_LOGINROOM *lr) {
    struct player *m = login(s, source, lr->roomid, 0.5f, &lr->detail);
    if (m) {
        sh_trace("Room %u robot %u login ok", lr->roomid, lr->detail.accid);
        ai_init(m, lr->ai);
        notify_game_info(s, m);
        loadok(s, m);
    } else {
        sh_trace("Room %u robot %u login fail", lr->roomid, lr->detail.accid);
    }
}

static void
sync_position(struct module *s, struct player *m, const struct UM_GAMESYNC *sync) {
    struct room_game *ro = room_member_to_game(m);
    if (m->depth == sync->depth) {
        return;
    }
    m->depth = sync->depth;
    multicast_msg(s, ro, sync, sizeof(*sync), UID(m));

    if (m->depth >= ro->map->height) {
        if (ro->type == ROOM_TYPE_NORMAL) {
            member_over(s, ro, m, F_CLIENT|F_OTHER|F_AWARD);
        } else {
            game_over(s, ro, false);
        }
    }
}

static void
sync_press(struct module *s, struct player *m, const struct UM_ROLEPRESS *press) {
    struct room_game *ro = room_member_to_game(m);

    if (has_effect_state(m, EFFECT_STATE_PROTECT_ONCE)) {
        clr_effect_state(m, EFFECT_STATE_PROTECT_ONCE);
    } else if (has_effect_state(m, EFFECT_STATE_PROTECT)) {
        return;
    }
    // todo 临时屏蔽
    //int oxygen = m->base.oxygen/10;
    //if (reduce_oxygen(m, oxygen) > 0) {
        //m->refresh_flag |= REFRESH_ATTRI;
    //}
    on_refresh_attri(s, m, ro);

    multicast_msg(s, ro, press, sizeof(*press), UID(m));

    if (m->detail.attri.oxygen <= 0) {
        if (ro->type == ROOM_TYPE_NORMAL) {
            member_over(s, ro, m, F_CLIENT|F_OTHER|F_AWARD);
        } else {
            game_over(s, ro, true);
        }
    }
}

struct buff_ud {
    struct module *s;
    struct room_game *ro;
    struct player *m;
};

static int
buff_delay_update(void *elem, void *ud) {
    struct buff_delay *d = elem;
    struct buff_ud *bu = ud;
    struct module *s = bu->s;
    struct room *self = MODULE_SELF;
    
    if (d->id == 0 ||
        d->first_time > sh_timer_now()) {
        return 0;
    }
    struct item_tplt *item = room_tplt_find_item(self, d->id);
    if (item) {
        item_effect(s, bu->ro, bu->m, item, d->last_time - d->first_time);
    }
    buff_delay_del(d);
    return 0;
}

static int
buff_effect_update(void *elem, void *ud) {
    struct buff_effect *e = elem;
    struct buff_ud *bu = ud;
    struct module *s = bu->s;
    struct player *m = bu->m;

    if (e->id == 0 ||
        e->time > sh_timer_now()) {
        return 0;
    }
    sh_trace("timeout : %llu, to char %u", (unsigned long long)e->time, m->detail.charid);
    int i;
    for (i=0; i<BUFF_EFFECT; ++i) {
        e->effects[i].value *= -1;
        item_effectone(m, &e->effects[i]);
    }
    UM_DEFFIX(UM_ITEMUNEFFECT, uneffect);
    uneffect->charid = m->detail.charid;
    uneffect->itemid = e->id;
    multicast_msg(s, bu->ro, uneffect, sizeof(*uneffect), 0);

    buff_effect_del(e);
    return 0;
}

static void
room_game_update_delay(struct module *s, struct room_game *ro) {
    struct player *m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (is_online(m)) {
            struct buff_ud ud = {s, ro, m};
            sh_array_foreach(&m->total_delay, buff_delay_update, &ud);
        }
    }
}

static void
room_game_update(struct module *s, struct room_game *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (is_online(m)) {
            int oxygen = role_oxygen_time_consume(&ro->gattri, &m->detail.attri);
            if (reduce_oxygen(m, oxygen) > 0) {
                m->refresh_flag |= REFRESH_ATTRI;
            }
            struct buff_ud ud = {s, ro, m};
            sh_array_foreach(&m->total_effect, buff_effect_update, &ud);
            on_refresh_attri(s, m, ro);
            if (is_robot(m)) {
                ai_main(s, ro, m);
            } else {
                uint64_t elapsed = sh_timer_now() - ro->starttime;
                if (elapsed > 0) {
                m->speed_old = m->speed_new;
                m->speed_new = m->depth / (elapsed/1000.f);
                }
            }
        }
    }
}

void
game_player_main(struct module *s, struct player *m, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_LOGOUT: {
        struct room_game *ro = room_member_to_game(m);
        member_over(s, ro, m, F_OTHER);
        }
        break;
    case IDUM_GAMELOADOK:
        loadok(s, m);
        break;
    case IDUM_GAMESYNC: {
        UM_CASTCK(UM_GAMESYNC, sync, base, sz);
        sync_position(s, m, sync);
        break;
        }
    case IDUM_ROLEPRESS: {
        UM_CASTCK(UM_ROLEPRESS, press, base, sz);
        sync_press(s, m, press);
        break;
        }
    case IDUM_USEITEM: {
        UM_CASTCK(UM_USEITEM, use, base, sz);
        use_item(s, m, use);
        break;
        }
    }
}

void
game_main(struct module *s, int source, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_LOGINROOM: {
        UM_CAST(UM_LOGINROOM, lo, msg);
        player_login(s, source, lo);
        break;
        }
    case IDUM_ROBOT_LOGINROOM: {
        UM_CAST(UM_ROBOT_LOGINROOM, rlo, msg);
        robot_login(s, source, rlo);
        break;
        }
    case IDUM_CREATEROOM: {
        UM_CAST(UM_CREATEROOM, create, msg);
        room_game_create(s, source, create);
        break;
        }
    case IDUM_DESTROYROOM: {
        UM_CAST(UM_DESTROYROOM, des, msg);
        room_game_destroy_byid(s, des->id);
        break;
        }
    case IDUM_JOINROOM: {
        UM_CAST(UM_JOINROOM, join, msg);
        room_game_join(s, source, join->id, &join->mm);
        break;
        }
    }
}

static void
timecb_1(void *pointer, void *ud) {
    struct module *s = ud;
    struct room_game *ro = pointer;
    if (ro->status == ROOMS_START) {
        room_game_update_delay(s, ro);
    }
}

static void
timecb_2(void *pointer, void *ud) {
    struct module *s = ud;
    struct room_game *ro = pointer;
    switch (ro->status) {
    case ROOMS_CREATE:
        check_enter_room_game(s, ro);
        break;
    case ROOMS_ENTER:
        check_start_room_game(s, ro);
        break;
    case ROOMS_START:
        room_game_update(s, ro);
        check_over_room_game(s, ro);
        break;
    case ROOMS_OVER:
        check_destroy_room_game(s, ro);
        break;
    }
}

void
game_time(struct module* s) {
    struct room* self = MODULE_SELF;
    if (SEC_ELAPSED(0.1)) {
        sh_hash_foreach2(&self->room_games, timecb_1, s);
    }
    if (SEC_ELAPSED(1)) {
        sh_hash_foreach2(&self->room_games, timecb_2, s);
    }
    self->tick++;
}

int  
game_init(struct room *self) {
    uint32_t now = sh_timer_now()/1000;
    self->randseed = now;
    self->map_randseed = now;

    sh_hash_init(&self->players, 1);
    sh_hash_init(&self->room_games, 1);
    return 0;
}

void 
game_fini(struct room *self) {
    sh_hash_fini(&self->players);
    sh_hash_fini(&self->room_games);
}
