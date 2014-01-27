#include "sc_service.h"
#include "sc.h"
#include "sc_log.h"
#include "sh_util.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "sh_hash.h"

#include "tplt_include.h"
#include "tplt_struct.h"

#include "sharetype.h"
#include "cli_message.h"
#include "user_message.h"

#include "fight.h"
#include "map.h"
#include "roommap.h"
#include "genmap.h"
#include "mapdatamgr.h"
#include "buff.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 3000
#define DESTROY_TIMEOUT 500

// refresh data type, binary bit
#define REFRESH_SPEED 1 
#define REFRESH_ATTRI 2

#define AI_S_MOVE 0
#define AI_S_FOCUS 1

struct AI_target {
    uint32_t id;
    uint16_t w;
    uint16_t h;
    uint16_t block;
};

struct AI_brain {
    int level;
    uint64_t last_execute_time;
    float accu_depth; // 累积的距离
    int status;
    uint64_t status_intv;
    uint64_t status_time;
    int dir;
    float speed;
    struct AI_target target;
    struct AI_target target2;
    uint64_t press_time;
    int tick;
};

struct player {
    int watchdog_source;
    uint8_t index;
    bool isrobot;
    bool login;
    bool online;
    bool loadok;
    int refresh_flag;
    struct tmemberdetail detail;
    struct char_attribute base;
    struct delay_vector total_delay;
    struct effect_vector total_effect;
    int32_t depth;
    uint64_t deathtime;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t ntrap;
    int16_t nbao;
    int16_t nbedamage;
    float speed_new;
    float speed_old;
    struct AI_brain *brain;
};

struct gameroom { 
    uint32_t id;
    int8_t type; // ROOM_TYPE*
    //uint32_t key;
    int status; // ROOMS_*
    uint64_t statustime;
    uint64_t starttime;
    int np;
    struct player p[MEMBER_MAX];
    struct groundattri gattri;
    struct genmap* map;
};

#define member2gameroom(m) ({ \
    assert(m->index >=0 && m->index < MEMBER_MAX); \
    ((struct gameroom*)((char*)(m) - (m)->index * sizeof(*(m)) - offsetof(struct gameroom, p))); \
})

#define UID(m) ((m)->detail.accid)

static inline float
oxygen_percent(struct player *pr) {
    return pr->detail.attri.oxygen / (float)pr->base.oxygen;
}

struct room {
    //int watchdog_handle;
    //int match_handle;
    int tick;
    uint32_t randseed;
    uint32_t map_randseed;
    struct sh_hash players;
    struct sh_hash gamerooms;
};

struct room*
room_create() {
    struct room* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

static void
member_free(struct player* m) {
    delay_fini(&m->total_delay);
    effect_fini(&m->total_effect);
    if (m->brain) {
        free(m->brain);
        m->brain = NULL;
    }
}

static void
free_gameroom(void *pointer) {
    struct gameroom *ro = pointer;
    if (ro->map) {
        genmap_free(ro->map);
        ro->map = NULL;
    }
    int i;
    for (i=0; i<ro->np; ++i) {
        member_free(&ro->p[i]);
    }
    free(ro);
}

void
room_free(struct room* self) {
    if (self == NULL)
        return;
    sh_hash_fini(&self->players);
    sh_hash_foreach(&self->gamerooms, free_gameroom);
    sh_hash_fini(&self->gamerooms);
    free(self);
}

int
room_init(struct service* s) {
    struct room* self = SERVICE_SELF;

    if (sh_handle_publish(SERVICE_NAME, PUB_SER)) {
        return 1;
    }
    int handle;
    if (sh_handler("robot", SUB_REMOTE, &handle) ||
        sh_handler("watchdog", SUB_REMOTE, &handle) ||
        sh_handler("match", SUB_REMOTE, &handle))
        return 1;
    
    uint32_t now = sc_timer_now()/1000;
    self->randseed = now;
    self->map_randseed = now;

    sh_hash_init(&self->players, 1);
    sh_hash_init(&self->gamerooms, 1);

    sc_timer_register(SERVICE_ID, 100);
    return 0;
}

static inline bool
elapsed(uint64_t t, uint64_t elapse) {
    uint64_t now = sc_timer_now();
    return now > t && (now - t >= elapse);
}

static inline struct item_tplt*
itemtplt_find(uint32_t itemid) {
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_ITEM);
    if (vist) {
        return tplt_visitor_find(vist, itemid);
    }
    return NULL;
}

static inline const struct map_tplt*
maptplt_find(uint32_t mapid) {
    const struct tplt_visitor* visitor = tplt_get_visitor(TPLT_MAP);
    if (visitor) 
        return tplt_visitor_find(visitor, mapid);
    return NULL; 
}

static inline void
notify_create_gameroom_result(struct service *s, int dest_handle, uint32_t roomid, int err) {
    sc_trace("Room %u notify match create result %d", roomid, err);
    UM_DEFFIX(UM_CREATEROOMRES, result);
    result->id = roomid;
    result->err = err;
    sh_service_send(SERVICE_ID, dest_handle, MT_UM, result, sizeof(*result));
}

static void
multicast_msg(struct service *s, struct gameroom* ro, const void *msg, int sz, uint32_t except) {
    UM_DEFWRAP2(UM_CLIENT, cl, sz); 
    memcpy(cl->wrap, msg, sz);
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (UID(m) != except &&
            m->online &&
            m->brain == NULL) {
            cl->uid = UID(m);
            sh_service_send(SERVICE_ID, m->watchdog_source, MT_UM, cl, sizeof(*cl)+sz);
        }
    }
}

static void
logout(struct service *s, struct player *m) {
    if (!m->online) {
        return;
    }
    struct room *self = SERVICE_SELF;

    struct gameroom *ro = member2gameroom(m);
    uint32_t charid = m->detail.charid; 

    UM_DEFFIX(UM_EXITROOM, exit);
    exit->uid = UID(m);
    sh_service_send(SERVICE_ID, m->watchdog_source, MT_UM, exit, sizeof(*exit));

    if (ro->status == ROOMS_START) {
        UM_DEFFIX(UM_GAMEUNJOIN, unjoin);
        unjoin->charid = charid;
        multicast_msg(s, ro, unjoin, sizeof(*unjoin), UID(m));
    }
    m->online = false;
    member_free(m);
    
    sh_hash_remove(&self->players, UID(m));
}

//////////////////////////////////////////////////////////////////////
// gameroom logic

struct player *
member_get(struct gameroom *ro, uint32_t accid) {
    int i;
    for (i=0; i<ro->np; ++i) {
        if (ro->p[i].detail.accid == accid)
            return &ro->p[i];
    }
    return NULL;
}

static inline void
member_place(struct player *m, uint32_t accid, uint8_t index) {
    memset(m, 0, sizeof(*m));
    m->index = index;
    m->detail.accid = accid;
}

static void
gameroom_create(struct service *s, int source, struct UM_CREATEROOM *create) {
    sc_trace("Room %u recevie create from mapid %u", create->id, create->mapid);
    struct room* self = SERVICE_SELF;
    create->mapid = 1;
    struct gameroom *ro = sh_hash_find(&self->gamerooms, create->id);
    if (ro) {
        notify_create_gameroom_result(s, source, create->id, SERR_ROOMIDCONFLICT);
        return;
    }
    const struct map_tplt *mapt = maptplt_find(create->mapid); 
    if (mapt == NULL) {
        notify_create_gameroom_result(s, source, create->id, SERR_CRENOTPLT);
        return;
    }
    struct roommap *mapdata = mapdatamgr_find(mapt->id);
    if (mapdata == NULL) {
        notify_create_gameroom_result(s, source, create->id, SERR_CRENOMAP);
        return;
    }
    uint32_t map_randseed = self->map_randseed;
    struct genmap* mymap = genmap_create(mapt, mapdata, map_randseed);
    if (mymap == NULL) {
        notify_create_gameroom_result(s, source, create->id, SERR_CREMAP);
        return;
    }

    ro = malloc(sizeof(*ro));
    ro->id = create->id;
    ro->type = create->type;
    ro->map = mymap;
    ro->status = ROOMS_CREATE;
    ro->statustime = sc_timer_now();
    
    ro->gattri.randseed = map_randseed;
    ro->gattri.mapid = create->mapid;
    ground_attri_build(mapt->difficulty, &ro->gattri);

    int i;
    ro->np = create->nmember;
    for (i=0; i<create->nmember; ++i) {
        member_place(&ro->p[i], create->members[i], i);
    }
    assert(!sh_hash_insert(&self->gamerooms, ro->id, ro));
    notify_create_gameroom_result(s, source, create->id, SERR_OK);
    self->map_randseed++; 
    sc_trace("Room %u mapid %u create ok", create->id, create->mapid);
}

static void
gameroom_destroy(struct service *s, struct gameroom *ro) {
    sc_trace("Room %u destroy", ro->id);
    struct room *self = SERVICE_SELF;
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (m->online) {
            sc_trace("Room %u destroy, then logout member %u", ro->id, UID(m));
            logout(s, m);
        }
    }
    sh_hash_remove(&self->gamerooms, ro->id);
    free_gameroom(ro);
}

static void
gameroom_destroy_byid(struct service *s, uint32_t roomid) {
    sc_trace("Room %u destroy by id", roomid);
    struct room *self = SERVICE_SELF;
    struct gameroom *ro = sh_hash_find(&self->gamerooms, roomid);
    if (ro) {
        gameroom_destroy(s, ro);
    }
}

static void
gameroom_enter(struct service *s, struct gameroom* ro) {
    sc_trace("Room %u multi enter", ro->id); 
    ro->status = ROOMS_ENTER;
    ro->statustime = sc_timer_now();
    UM_DEFFIX(UM_GAMEENTER, enter);
    multicast_msg(s, ro, enter, sizeof(*enter), 0);
}

static void
gameroom_start(struct service *s, struct gameroom* ro) {
    sc_trace("Room %u multi start", ro->id); 
    ro->status = ROOMS_START; 
    ro->starttime = sc_timer_now();
    ro->statustime = ro->starttime;
    UM_DEFFIX(UM_GAMESTART, start);
    multicast_msg(s, ro, start, sizeof(*start), 0);
}

static int 
rankcmp(const void* p1, const void* p2) {
    const struct player* m1 = *(const struct player**)p1; 
    const struct player* m2 = *(const struct player**)p2;
    return m2->detail.attri.oxygen - m1->detail.attri.oxygen;
}

static int 
rankcmp2(const void* p1, const void* p2) {
    const struct player* m1 = *(const struct player**)p1; 
    const struct player* m2 = *(const struct player**)p2;
    return m2->depth - m1->depth;
}

static void
build_awards(struct gameroom *ro, struct player **sortm, int n, struct memberaward *awards) {
    struct player* m;
    int i;
    uint64_t gametime = sc_timer_now() - ro->starttime;
    if (gametime < 1000)
        gametime = 1000;

    int score_sum = 0;
    for (i=1; i<ro->np; ++i) {
        m = sortm[i];
        score_sum += m->detail.score_dashi;
    }
    int score_diff;
    if (ro->np > 0)
        score_diff = abs(sortm[0]->detail.score_dashi - score_sum/ro->np);
    else
        score_diff = 0;
    
    struct extra_first {
        int nitem;
        int ntrap;
        int nbedamage;
    };
    struct extra_first ef;
    memset(&ef, 0, sizeof(ef));
    for (i=0; i<ro->np; ++i) {
        m = sortm[i];
        if (m->nitem > ef.nitem)
            ef.nitem = m->nitem;      
        if (m->ntrap > ef.ntrap)
            ef.ntrap = m->ntrap;
        if (m->nbedamage > ef.nbedamage)
            ef.nbedamage = m->nbedamage;
    } 
    int score_depth, score_speed, score_oxygen, score_item, score_bao;
    int score, coin, exp;
    int cut_score, extra_score;
    const int score_line1 = 1000;
    const int score_line2 = 2000;
    float coin_profit, score_profit;
    struct char_attribute* a = &m->detail.attri;
    for (i=0; i<ro->np; ++i) {
        m = sortm[i];
        a = &m->detail.attri;
        score_depth = pow(m->depth, 0.5) * 100;
        score_speed = pow(m->depth/(gametime*0.001), 1.2) * 766;
        score_oxygen = pow(m->noxygenitem, 1.2) * 20;
        score_item = (m->nitem + m->ntrap) * 20;
        score_bao = pow(m->nbao, 1.5) * 100;
        
        coin_profit = 1+a->coin_profit;
        if (i==0)
            coin_profit += a->wincoin_profit+0.05;
        coin = (score_depth + score_speed + score_oxygen) * 0.1 * coin_profit;
        
        exp = (m->depth * 0.2 + m->nbao);
        if (ro->type == ROOM_TYPE_DASHI) {
            if (m->detail.score_dashi < score_line1)
                cut_score = 0;
            else {
                int t = min(score_line2, max(200, (m->detail.score_dashi+score_line1-score_line2)));
                cut_score = (t/200) * 200;
                if (cut_score < t)
                    cut_score += 1;
            }
            extra_score = 0;
            if (m->nitem < ef.nitem)
                extra_score++;
            if (m->ntrap >= ef.ntrap)
                extra_score++;
            if (m->nbedamage >= ef.nbedamage)
                extra_score++;
            if (i == 0) {
                score = max(3, min(20, 10 - score_diff * 0.05 - cut_score)) + extra_score;
            } else {
                score = max(-3, min(-15, -10 - score_diff * 0.1 - cut_score)) + extra_score;
            }
        }  else {
            score_profit = 1+a->score_profit;
            if (i==0)
                score_profit += a->winscore_profit + 0.05;
            score = score_depth + score_speed + score_oxygen + score_item + score_bao;
            score = score * score_profit * 10;
        }
        awards[i].exp = exp;
        awards[i].coin = coin;
        awards[i].score = score;
    }
}

static void
game_over(struct service *s, struct gameroom* ro, bool death) {
    if (ro->status == ROOMS_OVER) {
        return;
    }
    sc_trace("Room %u over, by death? %d", ro->id, death); 
    struct player* m;
    struct player* sortm[MEMBER_MAX];
    struct memberaward awards[MEMBER_MAX];
    int i;
    // rank sort
    for (i=0; i<ro->np; ++i) {
        sortm[i] = &ro->p[i];
    }
    if (death) {
        qsort(sortm, ro->np, sizeof(sortm[0]), rankcmp);
    } else {
        qsort(sortm, ro->np, sizeof(sortm[0]), rankcmp2);
    }

    // award to hall
    build_awards(ro, sortm, ro->np, awards);
    for (i=0; i<ro->np; ++i) {
        m = sortm[i];
        if (m->online &&
            m->brain == NULL) {
            UM_DEFWRAP(UM_HALL, ha, UM_GAMEAWARD, ga);
            ha->uid  = UID(m);
            ga->type = ro->type;
            ga->award = awards[i];
            sh_service_send(SERVICE_ID, m->watchdog_source, MT_UM, ha, sizeof(*ha)+sizeof(*ga));
        }
    }

    // overinfo to client
    UM_DEFVAR(UM_CLIENT, umro);
    UD_CAST(UM_GAMEOVER, go, umro->wrap);
    go->type = ro->type;
    go->nmember = ro->np;
    for (i=0; i<ro->np; ++i) {
        m = sortm[i];
        go->stats[i].charid = m->detail.charid;
        go->stats[i].depth = m->depth;
        go->stats[i].noxygenitem = m->noxygenitem;
        go->stats[i].nitem = m->nitem;
        go->stats[i].nbao = m->nbao;
        go->stats[i].exp = awards[i].exp;
        go->stats[i].coin = awards[i].coin;
        go->stats[i].score = awards[i].score;
    }  
    int oversz = UM_GAMEOVER_size(go);
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        umro->uid = UID(m);
        if (m->online &&
            m->brain == NULL) {
            sh_service_send(SERVICE_ID, m->watchdog_source, MT_UM, umro, sizeof(*umro)+oversz);
        }
    }

    // over gameroom
    ro->status = ROOMS_OVER;
    ro->statustime = sc_timer_now();
}

static bool
is_total_loadok(struct gameroom *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        if (!ro->p[i].loadok)
            return false;
    }
    return true;
}

static int
count_onlinemember(struct gameroom* ro) {
    int i, n=0;
    for (i=0; i<ro->np; ++i) {
        if (ro->p[i].online)
            n++;
    }
    return n;
}

static bool
check_enter_gameroom(struct service *s, struct gameroom *ro) {
    if (ro->status != ROOMS_CREATE) {
        return false;
    }
    if (!elapsed(ro->statustime, ENTER_TIMELEAST)) {
        return false;
    }
    if (is_total_loadok(ro)) {
        gameroom_enter(s, ro);
        return true;
    } 
    if (!elapsed(ro->statustime, ENTER_TIMEOUT)) {
        return false;
    }
    if (count_onlinemember(ro) > 0) {
        gameroom_enter(s, ro);
        return true;
    } else {
        gameroom_destroy(s, ro);
        return false;
    }
    return false;
}

static bool
check_start_gameroom(struct service *s, struct gameroom *ro) {
    if (ro->status != ROOMS_ENTER) {
        return false;
    }
    if (elapsed(ro->statustime, START_TIMEOUT)) {
        gameroom_start(s, ro);
        return true;
    }
    return false;
}

static bool
someone_death(struct gameroom *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (m->online) {
            if (m->detail.attri.oxygen == 0)
                return true;
        }
    }
    return false;
}

static bool
check_over_gameroom(struct service *s, struct gameroom* ro) {
    if (ro->status != ROOMS_START) {
        return false;
    }
    int n = count_onlinemember(ro);
    if (n == 0) {
        gameroom_destroy(s, ro);
        return true;
    }
    if (someone_death(ro)) {
        game_over(s, ro, true);
        return true;
    }
    return false;
}

static bool
check_destroy_gameroom(struct service *s, struct gameroom *ro) {
    if (ro->status != ROOMS_OVER) {
        return false;
    }
    if (elapsed(ro->statustime, DESTROY_TIMEOUT)) {
        gameroom_destroy(s, ro);
        return true;
    }
    return false;
}


//////////////////////////////////////////////////////////////////////
static void
notify_game_info(struct service *s, struct player *m) {
    struct gameroom *ro = member2gameroom(m);

    if (m->brain == NULL) {
        UM_DEFVAR(UM_CLIENT, cl); 
        UD_CAST(UM_GAMEINFO, gi, cl->wrap);
        cl->uid = UID(m);
        gi->status = ro->status;
        gi->gattri = ro->gattri;
        
        int i, n=0;
        for (i=0; i<ro->np; ++i) {
            if (ro->p[i].online) {
                gi->members[i] = ro->p[i].detail;
                n++;
            }
        }
        gi->nmember = n;
        int sz = UM_GAMEINFO_size(gi);
        sh_service_send(SERVICE_ID, m->watchdog_source, MT_UM, cl, sizeof(*cl) + sz);
    }
    UM_DEFFIX(UM_GAMEMEMBER, gm);
    gm->member = m->detail;
    multicast_msg(s, ro, gm, sizeof(*gm), UID(m));
} 

static void
on_refresh_attri(struct service *s, struct player* m, struct gameroom* ro) {
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
    default:return 0.0f;
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

static void dump_ground(const struct groundattri *ga) {
    /*sc_rec("mapid: %u", ga->mapid);
    sc_rec("difficulty: %d", ga->difficulty);
    sc_rec("shaketime: %d", ga->shaketime);
    sc_rec("cellfallspeed: %f", ga->cellfallspeed);
    sc_rec("waitdestroy: %d", ga->waitdestroy);
    sc_rec("destroytime: %d", ga->destroytime);
    */
}

static void dump(uint32_t accid, const char* name, struct char_attribute* attri) {
    /*sc_rec("accid: id %u, name %s", accid, name);
    sc_rec("oxygen: %d", attri->oxygen);     // 氧气
    sc_rec("body: %d", attri->body);       // 体能
    sc_rec("quick: %d", attri->quick);      // 敏捷
    
    sc_rec("movespeed: %f", attri->movespeed);     // 移动速度
    sc_rec("movespeedadd: %f", attri->movespeedadd);
    sc_rec("charfallspeed: %f", attri->charfallspeed); // 坠落速度
    sc_rec("charfallspeedadd: %f", attri->charfallspeedadd);
    sc_rec("jmpspeed: %f", attri->jmpspeed);      // 跳跃速度--
    sc_rec("jmpacctime: %d", attri->jmpacctime);  // 跳跃准备时间--
    sc_rec("rebirthtime: %d", attri->rebirthtime); // 复活时间
    sc_rec("rebirthtimeadd: %f", attri->rebirthtimeadd);
    sc_rec("dodgedistance: %f", attri->dodgedistance); // 闪避距离
    sc_rec("dodgedistanceadd: %f", attri->dodgedistanceadd);
    sc_rec("jump_range: %d", attri->jump_range);  // 跳跃高度
    sc_rec("sence_range: %d", attri->sence_range); // 感知范围
    sc_rec("view_range: %d", attri->view_range);  // 视野范围
   
    sc_rec("attack_power: %d", attri->attack_power);
    sc_rec("attack_distance: %d", attri->attack_distance);
    sc_rec("attack_range: %d", attri->attack_range);
    sc_rec("attack_speed: %d", attri->attack_speed);

    sc_rec("coin_profit: %f", attri->coin_profit);
    sc_rec("wincoin_profit: %f", attri->wincoin_profit);
    sc_rec("score_profit: %f", attri->score_profit);
    sc_rec("winscore_profit: %f", attri->winscore_profit);
    sc_rec("exp_profit: %f", attri->exp_profit);
    sc_rec("item_timeadd: %f", attri->item_timeadd);
    sc_rec("item_oxygenadd: %f", attri->item_oxygenadd);
    sc_rec("lucky: %d", attri->lucky);
    sc_rec("prices: %d", attri->prices);
    */
}


static void
item_effect_member(struct service *s, struct gameroom *ro, struct player *m, 
        const struct item_tplt* item, int addtime) {
    struct one_effect tmp[BUFF_EFFECT];
    struct one_effect* effectptr = NULL;
    struct buff_effect *effect;

    if (item->time > 0) {
        effect = effect_find(&m->total_effect, item->id);
        if (effect == NULL) {
            effect = effect_add(&m->total_effect, item->id);
            assert(effect);
            effectptr = effect->effects;
        }
        effect->time = sc_timer_now()/1000 + item->time + addtime;
        sc_debug("insert time: %u, to char %u", effect->time, m->detail.charid);
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
        //dump(charid(m), m->detail.name, &m->detail.attri);
    }
}

static inline int 
get_effect_members(struct gameroom* ro, 
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
            if (m->online && m != me) {
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
            if (m->online) {
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
item_effect(struct service* s, struct gameroom* ro, struct player* me, 
        const struct item_tplt* item, int addtime) {
    sc_debug("char %u, item effect %u", me->detail.charid, item->id);

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
    sc_debug("char %u, use delay item %u", m->detail.charid, item->id);

    uint64_t effect_time = sc_timer_now() + delay_time;
    struct buff_delay* delay = delay_find(&m->total_delay, item->id);
    if (delay == NULL) {
        delay = delay_add(&m->total_delay, item->id);
        assert(delay);
        delay->first_effect_time = effect_time;
    } 
    delay->last_effect_time = effect_time;
}

static uint32_t
rand_baoitem(struct room *self, const struct item_tplt *item, const struct map_tplt *mapt) {
#define CASE_BAO(n) case n: \
    return mapt->baoitem ## n[sc_rand(self->randseed)%mapt->nbaoitem ## n] \

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
rand_fightitem(const struct map_tplt *mapt) {
    uint32_t randid = mapt->fightitem[rand()%mapt->nfightitem];
    return itemtplt_find(randid);
}

static inline const struct item_tplt*
rand_trapitem(const struct map_tplt *mapt) {
    uint32_t randid = mapt->trapitem[rand()%mapt->ntrapitem];
    const struct item_tplt* item = itemtplt_find(randid);
    if (item == NULL) {
        sc_debug("not found rand item %u", randid);
    }
    return item;
}

static void
use_item(struct service *s, struct player *m, const struct UM_USEITEM *use) {
    struct room *self = SERVICE_SELF;

    struct gameroom *ro = member2gameroom(m);

    const struct item_tplt* item = itemtplt_find(use->itemid);
    if (item == NULL) {
        sc_debug("not found use item: %u", use->itemid);
        return;
    }
    const struct item_tplt* oriitem = item;
    const struct map_tplt* tmap = maptplt_find(ro->gattri.mapid); 
    if (tmap == NULL) {
        return;
    }
    int i;
    switch (item->type) {
    case ITEM_T_OXYGEN:
        m->noxygenitem += 1;
        break;
    case ITEM_T_FIGHT:
        if (item->subtype == 0) {
            item = rand_fightitem(tmap);
            if (item == NULL) {
                return;
            }
            m->nitem += 1;
        }
        break;
    case ITEM_T_TRAP:
        if (item->subtype == 0) {
            item = rand_trapitem(tmap);
            if (item == NULL) {
                return;
            }
        }
        break;
    case ITEM_T_BAO: {
        uint32_t baoid = rand_baoitem(self, item, tmap);
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
        for (i=0; i<ntar; ++i) {
            struct player *one = tars[i];
            item_effect_member(s, ro, one, item, 0);
        }
    }

    UM_DEFFIX(UM_ITEMEFFECT, ie);
    ie->spellid = m->detail.charid;
    ie->oriitem = oriitem->id;
    ie->itemid = item->id;
    for (i=0; i<ntar; ++i) {
        struct player *one = tars[i];
        ie->charid = one->detail.charid; 
        multicast_msg(s, ro, ie, sizeof(*ie), 0);
    }
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
member_update_delay(struct service *s, struct gameroom *ro, struct player *m, uint64_t now) {
    struct buff_delay *delay;
    struct item_tplt *item;
    int i;
    for (i=0; i<m->total_delay.sz; ++i) {
        delay = &m->total_delay.p[i];
        if (delay->id == 0) {
            continue;
        }
        if (delay->first_effect_time > sc_timer_now()) {
            continue;
        }
        item = itemtplt_find(delay->id);
        if (item) {
            int add_time = 
                delay->last_effect_time > delay->first_effect_time ?
                delay->last_effect_time - delay->first_effect_time : 0;
            item_effect(s, ro, m, item, add_time);
        }
        delay_del(delay);
    }
}

static void
gameroom_update_delay(struct service *s, struct gameroom *ro) {
    uint64_t now = sc_timer_now();
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (m->online) {
            member_update_delay(s, ro, m, now);
        }
    }
}

static void
member_update_effect(struct player *m) {
    struct buff_effect *effect;
    int i, j;
    for (i=0; i<m->total_effect.sz; ++i) {
        effect = &m->total_effect.p[i];
        if (effect->id == 0)
            continue;

        if (effect->time > 0 &&
            effect->time <= sc_timer_now()/1000) {
            sc_debug("timeout : %u, to char %u", effect->time, m->detail.charid);
            
            for (j=0; j<BUFF_EFFECT; ++j) {
                effect->effects[j].value *= -1;
                item_effectone(m, &effect->effects[j]);
            }
            effect_del(effect);
        }
    }
}

static void
loadok(struct service *s, struct player *m) {
    if (!m->loadok) {
        struct gameroom *ro = member2gameroom(m);
        m->loadok = true;
        check_enter_gameroom(s, ro);
    }
}

static struct player *
login(struct service *s, int source, uint32_t roomid, const struct tmemberdetail *detail) {
    struct room *self = SERVICE_SELF;

    uint32_t accid  = detail->accid;
    struct gameroom *ro = sh_hash_find(&self->gamerooms, roomid); 
    if (ro == NULL) {
        return NULL; // someting wrong
    }
    struct player *m = member_get(ro, accid);
    if (m == NULL || m->online) {
        return NULL; // someting wrong
    }
    m->detail = *detail; 
    role_attri_build(&ro->gattri, &m->detail.attri);
    m->base = m->detail.attri;
dump_ground(&ro->gattri);
dump(accid, m->detail.name, &m->detail.attri);
    delay_init(&m->total_delay);
    effect_init(&m->total_effect);

    m->online = true;
    m->watchdog_source = source;
    m->brain = NULL; 
    assert(!sh_hash_insert(&self->players, accid, m));
    return m;
}

static void
player_login(struct service *s, int source, const struct UM_LOGINROOM *lr) { 
    struct player *m = login(s, source, lr->roomid, &lr->detail);
    if (m) {
        sc_trace("Room %u player %u login ok", lr->roomid, lr->detail.accid);
        m->brain = NULL;
        notify_game_info(s, m);
    } else {
        sc_trace("Room %u player %u login fail", lr->roomid, lr->detail.accid);
    }
}

static void
robot_login(struct service *s, int source, const struct UM_ROBOT_LOGINROOM *lr) {
    struct player *m = login(s, source, lr->roomid, &lr->detail);
    if (m) {
        sc_trace("Room %u robot %u login ok", lr->roomid, lr->detail.accid);
        struct AI_brain *brain = malloc(sizeof(*brain));
        memset(brain, 0, sizeof(*brain));
        brain->level = lr->level;
        m->brain = brain;

        notify_game_info(s, m);
        loadok(s, m);
    } else {
        sc_trace("Room %u robot %u login fail", lr->roomid, lr->detail.accid);
    }
}

static void
sync_position(struct service *s, struct player *m, const struct UM_GAMESYNC *sync) {
    struct gameroom *ro = member2gameroom(m);
    if (m->depth == sync->depth) {
        return;
    }
    m->depth = sync->depth;
    multicast_msg(s, ro, sync, sizeof(*sync), UID(m));

    if (m->depth >= ro->map->height) {
        game_over(s, ro, false);
    }
}

static void
sync_press(struct service *s, struct player *m, const struct UM_ROLEPRESS *press) {
    struct gameroom *ro = member2gameroom(m);

    int oxygen = m->base.oxygen/10;
    if (reduce_oxygen(m, oxygen) > 0) {
        m->refresh_flag |= REFRESH_ATTRI;
    }
    on_refresh_attri(s, m, ro);

    multicast_msg(s, ro, press, sizeof(*press), UID(m));

    if (m->detail.attri.oxygen <= 0) {
        game_over(s, ro, true);
    }
}

//-------------------------------------------------------------------------------
static struct player *
get_front_player(struct gameroom *ro, struct player *pr) {
    struct player *m;
    int i;
    for (i=0; i<ro->np; ++i) { 
        m = &ro->p[i];
        if (m->online &&
            m != pr &&
            m->depth > pr->depth) {
            return m;
        }
    }
    return NULL;
}

static float 
fall_speed(struct player *pr) {
    const struct char_attribute *attr = &pr->base;
    return attr->charfallspeed; 
}

static int
AI_lookup(struct gameroom *ro, struct player *pr, int type, struct AI_target *target) {
    struct AI_brain *brain = pr->brain;
    assert(brain);

    struct genmap *map = ro->map;
    int32_t depth = pr->depth + 2; 
    if (depth <= 0 || depth > map->height) {
        return 1;
    }
    struct genmap_cell *cell;
    struct item_tplt *item;
    int i, start = (depth-1)*map->width;
    for (i=0; i<map->width; ++i) {
        cell = &map->cells[start+i];
        if (cell->cellid == 0 && cell->itemid > 0) {
            item = itemtplt_find(cell->itemid);
            if (item->type == type) {
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
AI_rand(int level, int rateup, int levelup, int ratedown, int leveldown) {
    float rate = (ratedown + 
                  (level - leveldown) * 
                  (float)(rateup-ratedown) / 
                  (float)(levelup-leveldown)) / 
        (float)(rand()%100);
    return rate >= 1;
}

static inline struct AI_target *
AI_current_target(struct AI_brain *brain) {
    if (brain->target.id > 0) {
        return &brain->target;
    }
    if (brain->target2.id > 0) {
        return &brain->target2;
    }
    return NULL;
}

static inline int
AI_target_depth(struct AI_target *target) {
    return target->h+1;
}

static int
AI_dir(struct player *pr) {
    struct AI_brain *brain = pr->brain;
    struct AI_target *target = AI_current_target(brain);
    if (target) {
        if (AI_target_depth(target) > pr->depth) {
            return 1;
        } else {
            return rand()%3 - 1;
        }
    } else {
        if (AI_rand(brain->level, 10, 1, 0, 10)) {
            return rand()%2 - 1;
        } else {
            return 1;
        }
    }
}

static inline float
AI_standard_speed(struct AI_brain *brain) {
    return (60+brain->level*20)/100.f;
}

static int
AI_down_block(struct gameroom *ro, int depth) {
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

static float
AI_speed(struct gameroom *ro, struct player *pr) {
    struct AI_brain *brain = pr->brain;
    if (brain->dir < 1)
        return 1;
    float speed;
    struct player *front = get_front_player(ro, pr);
    if (front) {
        if (front->speed_new > front->speed_old) {
            speed = AI_standard_speed(brain);
        } else {
            speed = fall_speed(pr);
        }
    } else {
        speed = AI_standard_speed(brain);
    }
    int buff_value = 0; // todo
    int down_block = AI_down_block(ro, pr->depth);
    return speed * (1+buff_value/100.f - down_block/100.f);
}

static uint64_t
AI_wander_time(int level, int block) {
    return block * (1 - level/40.f);
}

static void
AI_update_move(struct service *s, struct player *pr, float elapsed) {
    struct AI_brain *brain = pr->brain;
    sc_trace("AI %u update move (%d, %f)", UID(pr), brain->dir, brain->speed);
    if (brain->dir == 0) {
        return;
    }
    float dist = brain->dir * brain->speed * elapsed;
    brain->accu_depth += dist;
    int depth = brain->accu_depth;
    if (abs(depth) >= 1) {
        brain->accu_depth -= depth;
        depth += pr->depth;
        if (depth < 0)
            depth = 0;
        UM_DEFFIX(UM_GAMESYNC, sync);
        sync->charid = pr->detail.charid;
        sync->depth = depth;
        sync_position(s, pr, sync);
        sc_trace("AI %u update depth (%d, cur %d)", UID(pr), depth, pr->depth);
    } 
}

static inline void
AI_focus_target(struct player *pr, const struct AI_target *target) {
    struct AI_brain *brain = pr->brain;
    brain->status = AI_S_FOCUS;
    brain->status_time = sc_timer_now();
    brain->status_intv = AI_wander_time(brain->level, target->block); 
    sc_trace("AI %u switch to focus (%u in %u,%u b %u)", UID(pr), 
            target->id, 
            target->w, 
            target->h, 
            target->block);
}

static inline bool
AI_can_pick(struct player *pr) {
    struct AI_brain *brain = pr->brain;
    if (brain->target.id > 0) {
        int rate = 110 - (pr->depth - AI_target_depth(&brain->target)) / 0.4;
        sc_trace("AI %u pick target1 rate %d", UID(pr), rate);
        if (rate >= 100 ||
            rand()%100 <= rate) {
            return true;
        }
    } else if (brain->target2.id > 0) {
        sc_trace("AI %u pick target2 rate 100", UID(pr));
        return true;
    }
    return false;

}
static void
AI_pick_target(struct service *s, struct gameroom *ro, struct player *pr) {
    struct AI_brain *brain = pr->brain;
    struct AI_target *target = AI_current_target(brain);
    if (target) {
        sc_trace("AI %u rand pick target (%u in %u,%u)", UID(pr), 
            brain->target.id, 
            brain->target.w, 
            brain->target.h);
        if (AI_can_pick(pr)) {
            sc_trace("AI %u pick target %u ok", UID(pr), target->id);
            UM_DEFFIX(UM_USEITEM, use);
            use->itemid = target->id;
            use_item(s, pr, use);
        }
        // clear
        struct genmap_cell *cell = GENMAP_CELL(ro->map, target->w, target->h);
        cell->itemid = 0;
        target->id = 0;
    }
    if (brain->target2.id > 0) {
        AI_focus_target(pr, &brain->target2);
    } else {
        brain->status = AI_S_MOVE;
        brain->status_time = 0;
        brain->status_intv = 0; 
        sc_trace("AI %u switch to move", UID(pr));
    }
}

static int
AI_lookup_oxygen(struct gameroom *ro, struct player *pr) {
    struct AI_brain *brain = pr->brain;
    struct AI_target target;
    if (!AI_lookup(ro, pr, ITEM_T_OXYGEN, &target)) {
        float limit_per = 0.8 + 0.2*(1.2 - min(1.2, brain->level/7.0));
        float oxygen_per = oxygen_percent(pr);
        sc_trace("AI %u lookup oxygen (%u in %u,%u b %u) oxygen_per %f limit_per %f", 
                UID(pr), 
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
AI_lookup_item(struct gameroom *ro, struct player *pr) {
    struct AI_brain *brain = pr->brain;
    struct AI_target target;
    if (!AI_lookup(ro, pr, ITEM_T_FIGHT, &target)) {
        sc_trace("AI %u lookup item (%u in %u,%u b %u)",
                UID(pr), 
                target.id,
                target.w,
                target.h,
                target.block);
        if (AI_rand(brain->level, 100, 6, 50, 1)) {
            brain->target = target; 
            return 0;
        }
    }
    return 1;
}

static void
AI_press(struct service *s, struct player *pr, uint64_t now, int press_time) {
    struct AI_brain *brain = pr->brain;
    brain->press_time = now;
    if (brain->status == AI_S_FOCUS) {
        brain->status_intv += press_time;
    }
    UM_DEFFIX(UM_ROLEPRESS, press);
    press->charid = pr->detail.charid;
    sync_press(s, pr, press);
    sc_trace("AI %u press begin", UID(pr));
}

static void
AI_main(struct service *s, struct gameroom *ro, struct player *pr) {
    struct AI_brain *brain = pr->brain;
    assert(brain);
    
    uint64_t now = sc_timer_now();
    if (brain->last_execute_time == 0 ||
        brain->last_execute_time > now) {
        brain->last_execute_time = now;
        brain->dir = 0;
        brain->speed = AI_speed(ro, pr);
        return;
    } 
    float elapsed = (now-brain->last_execute_time)/1000.f;
    if (elapsed == 0) {
        return;
    }

    sc_trace("AI %u level %d elapsed %f", UID(pr), brain->level, elapsed);
    int buff_value = 0; // todo

    bool trans = pr->depth%100 > 94;
    if (trans) { 
        float trans_speed = fall_speed(pr) * (1+buff_value/100.f);
        brain->dir = 1;
        brain->speed = trans_speed;
        sc_trace("AI %u [trans] speed %f depth %d", UID(pr), brain->speed, pr->depth);
        AI_update_move(s, pr, elapsed);
        goto AI_end;
    }

    if (brain->target.id == 0 && brain->target2.id == 0) {
        if (!AI_lookup_oxygen(ro, pr) ||
            !AI_lookup_item(ro, pr)) {
            AI_focus_target(pr, &brain->target);
        } 
    }
    if (brain->target2.id == 0) {
        if (!AI_lookup(ro, pr, ITEM_T_BAO, &brain->target2)) {
            if (brain->target.id == 0) {
                AI_focus_target(pr, &brain->target2);
            }
        }
    }

    if (brain->tick % 10 == 0) { 
        int press_time = pr->detail.attri.rebirthtime;
        if (brain->press_time == 0) {
            sc_trace("AI %u press rand", UID(pr));
            if (AI_rand(brain->level, 20, 1, 0, 8)) { 
                AI_press(s, pr, now, press_time);
                goto AI_end;
            }
        } else { 
            if (press_time <= (int)(now - brain->press_time)) {
                brain->press_time = 0;
                sc_trace("AI %u press end", UID(pr));
            }
            goto AI_end;
        } 
    }
    
    if (true/*brain->tick % 2 == 0*/) {
        brain->dir = AI_dir(pr);
        brain->speed = AI_speed(ro, pr);
    }
    AI_update_move(s, pr, elapsed);

    if (brain->status == AI_S_FOCUS) {
        if (now - brain->status_time >= brain->status_intv) {
            AI_pick_target(s, ro, pr);
        }
    }
AI_end:
    brain->last_execute_time = now;
    brain->tick++;
}

//-------------------------------------------------------------------------------
void
room_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    struct room *self = SERVICE_SELF;
    struct player *m;

    switch (type) {
    case MT_UM: {
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
        case IDUM_ROOM: {
            UM_CAST(UM_ROOM, ha, msg);
            UM_CAST(UM_BASE, wrap, ha->wrap);
 
            m = sh_hash_find(&self->players, ha->uid);
            if (m == NULL) {
                break;
            }
            switch (wrap->msgid) {
            case IDUM_LOGOUT:
                logout(s, m);
                break;
            case IDUM_GAMELOADOK:
                loadok(s, m);
                break;
            case IDUM_GAMESYNC: {
                UM_CAST(UM_GAMESYNC, sync, wrap);
                sync_position(s, m, sync);
                break;
                }
            case IDUM_ROLEPRESS: {
                UM_CAST(UM_ROLEPRESS, press, wrap);
                sync_press(s, m, press);
                break;
                }
            case IDUM_USEITEM: {
                UM_CAST(UM_USEITEM, use, wrap);
                use_item(s, m, use);
                break;
                }
            }
            break;
            }
        case IDUM_CREATEROOM: {
            UM_CAST(UM_CREATEROOM, create, msg);
            gameroom_create(s, source, create);
            break;
            }
        case IDUM_DESTROYROOM: {
            UM_CAST(UM_DESTROYROOM, des, msg);
            gameroom_destroy_byid(s, des->id);
            break;
            }
        }
        break;
        }
    case MT_MONITOR:
        // todo
        break;
    }
}

static void
gameroom_update(struct service *s, struct gameroom *ro) {
    int i;
    for (i=0; i<ro->np; ++i) {
        struct player *m = &ro->p[i];
        if (m->online) {
            int oxygen = role_oxygen_time_consume(&m->detail.attri);
            if (reduce_oxygen(m, oxygen) > 0) {
                m->refresh_flag |= REFRESH_ATTRI;
            }
            member_update_effect(m);
            on_refresh_attri(s, m, ro);
            if (m->brain) {
                AI_main(s, ro, m);
            } else {
                uint64_t elapsed = sc_timer_now() - ro->starttime;
                if (elapsed > 0) {
                    m->speed_old = m->speed_new;
                    m->speed_new = m->depth / (elapsed/1000.f);
                }
            }
        }
    }
}

static void
timecb(void *pointer, void *ud) {
    struct service *s = ud;
    struct room *self = SERVICE_SELF;
    struct gameroom *ro = pointer;
    switch (ro->status) {
    case ROOMS_START:
        gameroom_update_delay(s, ro);
        break;
    }
    if (self->tick % 10 == 0) {
        switch (ro->status) {
        case ROOMS_CREATE:
            check_enter_gameroom(s, ro);
            break;
        case ROOMS_ENTER:
            check_start_gameroom(s, ro);
            break;
        case ROOMS_START:
            gameroom_update(s, ro);
            check_over_gameroom(s, ro);
            break;
        case ROOMS_OVER:
            check_destroy_gameroom(s, ro);
            break;
        }
    }
}

void
room_time(struct service* s) {
    struct room* self = SERVICE_SELF;
    sh_hash_foreach2(&self->gamerooms, timecb, s);
    self->tick++;
}
