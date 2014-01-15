#include "sc_service.h"
#include "sc.h"
#include "sh_util.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "sc_gate.h"
#include "sharetype.h"
#include "node_type.h"
#include "gfreeid.h"
#include "cli_message.h"
#include "user_message.h"
#include "fight.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "map.h"
#include "roommap.h"
#include "genmap.h"
#include "sh_hash.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 3000
#define DESTROY_TIMEOUT 500

#define RS_CREATE 0
#define RS_ENTER  1
#define RS_START  2
#define RS_OVER   3

// refresh data type, binary bit
#define REFRESH_SPEED 1 
#define REFRESH_ATTRI 2

struct player {
    bool login;
    int roomid;
    uint32_t charid;
};

struct member {
    bool login;
    bool online;
    bool loadok;
    int connid; 
    int refresh_flag;
    struct tmemberdetail detail;
    struct char_attribute base;
    struct idmap* delaymap;
    struct idmap* buffmap;
    int32_t depth;
    uint64_t deathtime;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t ntrap;
    int16_t nbao;
    int16_t nbedamage;
};

struct room { 
    int id;
    int8_t type; // ROOM_TYPE*
    uint32_t key;
    int status; // RS_*
    uint64_t statustime;
    uint64_t starttime;
    int np;
    struct member p[MEMBER_MAX];
    struct groundattri gattri;
    struct genmap* map;
};

struct gfroom {
    GFREEID_FIELDS(room);
};

struct game {
    int tplt_handler;
    int tick;
    uint32_t randseed;
    uint32_t map_randseed;
    struct sh_hash players;
    struct sh_hash rooms;
};

struct buff_delay {
    uint64_t effect_time;
    uint64_t last_time;
};

struct buff_effect {
    int32_t type;
    bool isper;
    float value;
};

#define BUFF_EFFECT 3

struct buff {
    struct buff_effect effects[BUFF_EFFECT];
    int time;
};

static inline int
_sendto_client(struct member* m, struct UM_BASE* um, int sz) {
    if (m->online) {
       UM_SENDTOCLI(m->connid, um, sz); 
       return 0;
    }
    return 1;
}

static inline int
_sendto_world(struct UM_BASE* um, int sz) {
    const struct sc_node* node = sc_node_get(HNODE_ID(NODE_WORLD, 0));
    if (node) {
        UM_SENDTONODE(node, um, sz);
        return 0;
    }
    return 1;
}

static inline int
_get_oxygen(struct member* m) {
    return m->detail.attri.oxygen;
}

static inline int
_reduce_oxygen(struct member* m, int oxygen) {
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

struct game*
game_create() {
    struct game* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

static void
_freecb(void* value) {
    free(value);
}

static void
_freemember(struct member* m) {
    if (m->delaymap) {
        idmap_free(m->delaymap, _freecb);
        m->delaymap = NULL;
    }
    if (m->buffmap) {
        idmap_free(m->buffmap, _freecb);
        m->buffmap = NULL;
    }
}

void
game_free(struct game* self) {
    if (self == NULL)
        return;
    free(self->players);

    struct room* ro;
    struct member* m;
    int i, n;
    for (i=0; i<self->rooms.cap; ++i) {
        ro = &self->rooms.p[i];
        if (!ro->used)
            continue;
        for (n=0; n<ro->np; ++n) {
            m = &ro->p[n];
            if (m->online) {
                _freemember(m);
            }
        }
    }
    GFREEID_FINI(room, &self->rooms);
    free(self);
}

static inline struct item_tplt*
_get_item_tplt(struct game* self, uint32_t itemid) {
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_ITEM);
    if (vist) {
        return tplt_visitor_find(vist, itemid);
    }
    return NULL;
}

int
game_init(struct service* s) {
    struct game* self = SERVICE_SELF;
    if (sh_handler("tpltgame", &self->tplt_handler))
        return 1;
    int pmax = sc_gate_maxclient();
    if (pmax == 0) {
        sc_error("maxclient is zero, try load service gate before this");
        return 1;
    }
    self->pmax = pmax;
    self->players = malloc(sizeof(struct player) * pmax);
    memset(self->players, 0, sizeof(struct player) * pmax);
    // todo test this
    GFREEID_INIT(room, &self->rooms, 1);

    uint32_t now = sc_timer_now()/1000;
    self->randseed = now;
    self->map_randseed = now;

    SUBSCRIBE_MSG(s->serviceid, IDUM_CREATEROOM);
    SUBSCRIBE_MSG(s->serviceid, IDUM_CREATEROOMRES);

    sc_timer_register(s->serviceid, 100);
    return 0;
}

static inline struct player*
_allocplayer(struct game* self, struct gate_client* c) {
    int id = sc_gate_clientid(c);
    assert(id >= 0 && id < self->pmax);
    struct player* p = &self->players[id];
    assert(!p->login);
    return p;
}

static inline struct player*
_getplayer(struct game* self, struct gate_client* c) {
    int id = sc_gate_clientid(c);
    assert(id >= 0 && id < self->pmax);
    return &self->players[id];
}

static inline struct player*
_getonlineplayer(struct game* self, struct gate_client* c) {
    struct player* p = _getplayer(self, c);
    return p->login ? p : NULL;
}

static struct room*
_getroom(struct game* self, int roomid) {
    struct room* ro = GFREEID_SLOT(&self->rooms, roomid);
    return ro;
}

static inline void
_initmember(struct member* m, struct tmemberdetail* detail) {
    memset(m, 0, sizeof(*m));
    m->detail = *detail;
    m->base = m->detail.attri;
    m->connid = -1;
    m->delaymap = idmap_create(1);
    m->buffmap = idmap_create(1); 
}

static struct member*
_getmember(struct room* ro, uint32_t charid) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->detail.charid  == charid) {
            return m;
        }
    }
    return NULL;
}
static int
_count_onlinemember(struct room* ro) {
    struct member* m;
    int n = 0;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->login) {
            n++;
        }
    }
    return n;
}

static int
_count_loadokmember(struct room* ro) {
    struct member* m;
    int n = 0;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->loadok) {
            n++;
        }
    }
    return n;
}

static void
_verifyfail(struct gate_client* c, int8_t error) {
    UM_DEFFIX(UM_GAMELOGINFAIL, fail);
    fail->error = error;
    UM_SENDTOCLI(c->connid, fail, sizeof(*fail));
}
static inline bool
_elapsed(uint64_t t, uint64_t elapse) {
    uint64_t now = sc_timer_now();
    return now > t && (now - t >= elapse);
}

static void
_multicast_msg(struct room* ro, struct UM_BASE* um, uint32_t except) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->detail.charid != except &&
            m->online) {
            UM_SENDTOCLI(m->connid, um, um->msgsz);
        }
    }
}
static void
_logout(struct game* self, struct gate_client* c, bool disconn, bool multicast) {
    struct player* p = _getplayer(self, c);
    if (p == NULL) {
        return;
    }
    if (p->login) {
        int roomid = p->roomid;
        uint32_t charid = p->charid;
        p->login = false;
        p->roomid = 0;
        p->charid = 0;

        struct room* ro = _getroom(self, roomid);
        if (ro) {
            struct member* m = _getmember(ro, charid);
            if (m) {
                m->online = false;
                if (multicast) {
                    UM_DEFFIX(UM_GAMEUNJOIN, unjoin);
                    unjoin->charid = charid;
                    _multicast_msg(ro, (void*)unjoin, charid);
                }
                _freemember(m);
                if (disconn) {
                    sc_debug("disconnect : %u", m->detail.charid);
                    sc_gate_disconnclient(c, true);
                }
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////
// room logic

static struct room*
_create_room(struct game* self) {
    struct room* ro = GFREEID_ALLOC(room, &self->rooms);
    assert(ro);
    ro->status = RS_CREATE;
    ro->statustime = sc_timer_now();
    return ro;
}
static void
_enter_room(struct room* ro) {
    ro->status = RS_ENTER;
    ro->statustime = sc_timer_now();
    UM_DEFFIX(UM_GAMEENTER, enter);
    _multicast_msg(ro, (void*)enter, 0);
}
static void
_start_room(struct room* ro) {
    ro->status = RS_START; 
    ro->starttime = sc_timer_now();
    ro->statustime = ro->starttime;
    UM_DEFFIX(UM_GAMESTART, start);
    _multicast_msg(ro, (void*)start, 0);
}
static void
_destory_room(struct game* self, struct room* ro) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            _freemember(m);
            struct gate_client* c = sc_gate_getclient(m->connid);
            if (c) {
                _logout(self, c, true, false);
            }
        }
    }
    if (ro->map) {
        genmap_free(ro->map);
        ro->map = NULL;
    }
    GFREEID_FREE(room, &self->rooms, ro);
}
static bool
_check_enter_room(struct game* self, struct room* ro) {
    if (ro->status != RS_CREATE) {
        return false;
    }
    if (!_elapsed(ro->statustime, ENTER_TIMELEAST)) {
        return false;
    }
    int n = _count_loadokmember(ro);
    if (n == ro->np) {
        _enter_room(ro);
        return true;
    } else {
        if (_elapsed(ro->statustime, ENTER_TIMEOUT)) {
            if (_count_onlinemember(ro) > 0) {
                _enter_room(ro);
                return true;
            } else {
                _destory_room(self, ro);
                return false;
            }
        }
        return false;
    }
}
static bool
_check_start_room(struct game* self, struct room* ro) {
    if (ro->status != RS_ENTER) {
        return false;
    }
    if (_elapsed(ro->statustime, START_TIMEOUT)) {
        _start_room(ro);
        return true;
    }
    return false;
}
static int 
_rankcmp(const void* p1, const void* p2) {
    const struct member* m1 = *(const struct member**)p1; 
    const struct member* m2 = *(const struct member**)p2;
    return m2->detail.attri.oxygen - m1->detail.attri.oxygen;
}

static int 
_rankcmp2(const void* p1, const void* p2) {
    const struct member* m1 = *(const struct member**)p1; 
    const struct member* m2 = *(const struct member**)p2;
    return m2->depth - m1->depth;
}

static void
_build_awards(struct room* ro, struct member** sortm, int n, struct memberaward* awards) {
    struct member* m;
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
        awards[i].charid = m->detail.charid;
        awards[i].exp = exp;
        awards[i].coin = coin;
        awards[i].score = score;
    }
}

static void
_gameover(struct game* self, struct room* ro, bool death) {
    struct member* m;
    struct member* sortm[MEMBER_MAX];
    int i;
    // rank sort
    for (i=0; i<ro->np; ++i) {
        sortm[i] = &ro->p[i];
    }
    if (death) {
        qsort(sortm, ro->np, sizeof(sortm[0]), _rankcmp);
    } else {
        qsort(sortm, ro->np, sizeof(sortm[0]), _rankcmp2);
    }

    // to world
    UM_DEFVAR(UM_OVERROOM, or);
    or->type = ro->type;
    or->nmember = ro->np;
    _build_awards(ro, sortm, ro->np, or->awards);
    _sendto_world((void*)or, UM_OVERROOM_size(or));

    // to client
    UM_DEFVAR(UM_GAMEOVER, go);
    go->type = ro->type;
    go->nmember = ro->np;
    for (i=0; i<ro->np; ++i) {
        m = sortm[i];
        go->stats[i].charid = m->detail.charid;
        go->stats[i].depth = m->depth;
        go->stats[i].noxygenitem = m->noxygenitem;
        go->stats[i].nitem = m->nitem;
        go->stats[i].nbao = m->nbao;
        go->stats[i].exp = or->awards[i].exp;
        go->stats[i].coin = or->awards[i].coin;
        go->stats[i].score = or->awards[i].score;
    }  
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        _sendto_client(m, (void*)go, UM_GAMEOVER_size(go));
    }

    // over room
    ro->status = RS_OVER;
    ro->statustime = sc_timer_now();
}

static void
_check_over_room(struct game* self, struct room* ro) {
    if (ro->status != RS_START)
        return;
    int n = _count_onlinemember(ro);
    if (n == 0) {
        // todo: destroy directly
        _destory_room(self, ro);
        return;
    }
    struct member* m;
    bool isgameover = false;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            if (m->detail.attri.oxygen == 0) {
                isgameover = true;
                break;
            }
        }
    }
    if (!isgameover)
        return;

    _gameover(self, ro, true);
}

static void
_check_destory_room(struct game* self, struct room* ro) {
    if (ro->status != RS_OVER)
        return;
    if (!_elapsed(ro->statustime, DESTROY_TIMEOUT)) {
        return;
    }
    _destory_room(self, ro);
}

static inline const struct map_tplt*
maptplt_find(uint32_t mapid) {
    const struct tplt_visitor* visitor = tplt_get_visitor(TPLT_MAP);
    if (visitor) 
        return tplt_visitor_find(visitor, mapid);
    return NULL; 
}

static struct genmap*
create_map(struct game* self, const struct map_tplt* tplt, uint32_t seed) {
    struct service_message sm = { tplt->id, 0, sc_cstr_to_int32("GMAP"), 0, NULL };
    service_notify_service(self->tplt_handler, &sm);
    struct roommap* m = sm.result;
    if (m == NULL) {
        return NULL;
    }
    return genmap_create(tplt, m, seed);
}


//////////////////////////////////////////////////////////////////////
static void
_notify_gameinfo(struct gate_client* c, struct room* ro) {
    UM_DEFVAR(UM_GAMEINFO, ri);
    ri->gattri = ro->gattri;
    ri->nmember = ro->np;
    int i;
    for (i=0; i<ro->np; ++i) {
        ri->members[i] = ro->p[i].detail;
    }
    UM_SENDTOCLI(c->connid, ri, UM_GAMEINFO_size(ri));
} 
static void
_login(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    UM_CAST(UM_GAMELOGIN, login, um);
    struct room* ro = _getroom(self, login->roomid);
    if (ro == NULL) {
        _verifyfail(c, SERR_NOROOM);
        sc_gate_disconnclient(c, true);
        return;
    }
    if (login->roomkey != ro->key) {
        _verifyfail(c, SERR_ROOMKEY);
        sc_gate_disconnclient(c, true);
        return;
    }
    struct member* m = _getmember(ro, login->charid);
    if (m == NULL) {
        _verifyfail(c, SERR_NOMEMBER);
        sc_gate_disconnclient(c, true);
        return;
    }
    if (ro->status == RS_OVER) {
        _verifyfail(c, SERR_ROOMOVER);
        sc_gate_disconnclient(c, true);
        return;
    }
    struct player* p = _allocplayer(self, c);
    if (p == NULL) {
        _verifyfail(c, SERR_ALLOC);
        sc_gate_disconnclient(c, true);
        return;
    }
    sc_gate_loginclient(c);
    p->login = true;
    p->roomid = GFREEID_ID(ro, &self->rooms);
    p->charid = m->detail.charid;
    m->login = 1;
    m->online = 1;
    m->connid = c->connid; 

    _notify_gameinfo(c, ro);
    _check_enter_room(self, ro);
    return;
}
static inline int
_locate_player(struct game* self, struct gate_client* c, 
               struct player** p, 
               struct room** ro,
               struct member** m) {
    *p = _getonlineplayer(self, c);
    if (*p == NULL)
        return 1;
    *ro = _getroom(self, (*p)->roomid);
    if (*ro == NULL) 
        return 1;
    *m = _getmember(*ro, (*p)->charid);
    if (*m == NULL)
        return 1;
    return 0;
}

static void
_loadok(struct game* self, struct gate_client* c) {
    struct player* p;
    struct room* ro;
    struct member* m;
    if (_locate_player(self, c, &p, &ro, &m))
        return;
    if (!m->loadok) {
        m->loadok = true;
    }
}

static void
_sync(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p;
    struct room* ro;
    struct member* m;
    if (_locate_player(self, c, &p, &ro, &m))
        return;
    UM_CAST(UM_GAMESYNC, sync, um);
    m->depth = sync->depth;
    _multicast_msg(ro, (void*)sync, p->charid);

    if (m->depth >= ro->map->height) {
        _gameover(self, ro, false);
    }
}

static void
_on_refresh_attri(struct member* m, struct room* ro) {
    if (m->refresh_flag == 0)
        return;
    if (m->refresh_flag & REFRESH_SPEED) {
        role_attri_build(&ro->gattri, &m->detail.attri);
    }
    if (m->refresh_flag & REFRESH_ATTRI) {
        UM_DEFFIX(UM_ROLEINFO, ri);
        ri->detail = m->detail;
        _multicast_msg(ro, (void*)ri, 0);
    }
    m->refresh_flag = 0;
}

static void
_role_press(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p;
    struct room* ro;
    struct member* m;
    if (_locate_player(self, c, &p, &ro, &m))
        return;
    UM_CAST(UM_GAMEPRESS, gp, um);
    
    int oxygen = m->base.oxygen/10;
    if (_reduce_oxygen(m, oxygen) > 0) {
        m->refresh_flag |= REFRESH_ATTRI;
    }
    _on_refresh_attri(m, ro);

    _multicast_msg(ro, (void*)gp, p->charid);

    if (m->detail.attri.oxygen <= 0) {
        _gameover(self, ro, true);
    }
}

static float
_effect(struct member* m, struct char_attribute* cattri, const struct char_attribute* base, 
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
_item_effectone(struct member* m, struct buff_effect* effect) {
    if (effect->type > 0 && effect->value != 0) {
        return _effect(m, &m->detail.attri, &m->base, 
                effect->type, effect->value, effect->isper);
    }
    return 0;
}
/*
static void dump(uint32_t charid, const char* name, struct char_attribute* attri) {
    sc_rec("char: id %u, name %s", charid, name);
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
}
*/

static void
_item_effect_member(struct game* self, struct room* ro, struct member* m, 
        const struct item_tplt* titem, int addtime) {
    struct buff_effect tmp[BUFF_EFFECT];
    struct buff_effect* effectptr = NULL;
    struct buff* b = NULL;
    
    if (titem->time > 0) {
        b = idmap_find(m->buffmap, titem->id);
        if (b == NULL) {
            b = malloc(sizeof(*b));
            idmap_insert(m->buffmap, titem->id, b);
            effectptr = b->effects;
        } else {
            if (b->time == 0) {
                effectptr = b->effects;
            } else {
                // has effect, just recal time
            }
        }
        b->time = sc_timer_now()/1000 + titem->time + addtime;
        sc_debug("insert time: %u, to char %u", b->time, m->detail.charid);
    } else {
        effectptr = tmp;
    }
    if (effectptr) {
#define FILL_EFFECT(n) \
        if (n <= BUFF_EFFECT) { \
            effectptr[n-1].type  = titem->effect##n; \
            effectptr[n-1].isper = titem->valuet##n; \
            effectptr[n-1].value = titem->value##n;  \
        }
        FILL_EFFECT(1);
        FILL_EFFECT(2);
        FILL_EFFECT(3);

        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            effectptr[i].value = _item_effectone(m, &effectptr[i]);
            effectptr[i].isper = false;
        }

        _on_refresh_attri(m, ro);
        //dump(m->detail.charid, m->detail.name, &m->detail.attri);

    }
}

static inline int 
_get_effect_members(struct room* ro, 
                    struct member* me, 
                    int target, 
                    struct member* ret[MEMBER_MAX]) {
    switch (target) {
    case ITEM_TARGET_SELF:
        ret[0] = me;
        return 1;
    case ITEM_TARGET_ENEMY: {
        struct member* m;
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
        struct member* m;
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
_item_effect(struct game* self, struct room* ro, struct member* me, 
        const struct item_tplt* titem, int addtime) {
    sc_debug("char %u, titem effect %u", me->detail.charid, titem->id);

    struct member* tars[MEMBER_MAX];
    int ntar = _get_effect_members(ro, me, titem->target, tars);
    if (ntar <= 0) {
        return;
    }
    struct member* onetar;
    int i;
    for (i=0; i<ntar; ++i) {
        onetar = tars[i];
        _item_effect_member(self, ro, onetar, titem, addtime);
    }
}

static void
_item_delay(struct game* self, struct room* ro, struct member* m, const struct item_tplt* titem, int delay) {
    sc_debug("char %u, use delay titem %u", m->detail.charid, titem->id);

    struct buff_delay* bdelay = idmap_find(m->delaymap, titem->id);
    if (bdelay == NULL) {
        bdelay = malloc(sizeof(*bdelay));
        bdelay->effect_time = 0;
        idmap_insert(m->delaymap, titem->id, bdelay);
    }
    bdelay->last_time = sc_timer_now() + delay;
    if (bdelay->effect_time == 0) {
        bdelay->effect_time = bdelay->last_time;
    }
}

static uint32_t
_rand_baoitem(struct game* self, const struct item_tplt* titem, const struct map_tplt* tmap) {
#define CASE_BAO(n) case n: \
    return tmap->baoitem ## n[sc_rand(self->randseed)%tmap->nbaoitem ## n] \

    switch (titem->subtype) {
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
_rand_fightitem(struct game* self, const struct map_tplt* tmap) {
    uint32_t randid = tmap->fightitem[rand()%tmap->nfightitem];
    return _get_item_tplt(self, randid);
}

static inline const struct item_tplt*
_rand_trapitem(struct game* self, const struct map_tplt* tmap) {
    uint32_t randid = tmap->trapitem[rand()%tmap->ntrapitem];
    const struct item_tplt* titem = _get_item_tplt(self, randid);
    if (titem == NULL) {
        sc_debug("not found rand item %u", randid);
    }
    return titem;
}

static void
_use_item(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p;
    struct room* ro;
    struct member* me;
    if (_locate_player(self, c, &p, &ro, &me))
        return;

    UM_CAST(UM_USEITEM, useitem, um);
    const struct item_tplt* titem = _get_item_tplt(self, useitem->itemid);
    if (titem == NULL) {
        sc_debug("not found use item: %u", useitem->itemid);
        return;
    }
    const struct item_tplt* oriitem = titem;
    const struct map_tplt* tmap = maptplt_find(ro->gattri.mapid); 
    if (tmap == NULL) {
        return;
    }

    switch (titem->type) {
    case ITEM_T_OXYGEN:
        me->noxygenitem += 1;
        break;
    case ITEM_T_FIGHT:
        if (titem->subtype == 0) {
            titem = _rand_fightitem(self, tmap);
            if (titem == NULL) {
                return;
            }
            me->nitem += 1;
        }
        break;
    case ITEM_T_TRAP:
        if (titem->subtype == 0) {
            titem = _rand_trapitem(self, tmap);
            if (titem == NULL) {
                return;
            }
        }
        break;
    case ITEM_T_BAO: {
        uint32_t baoid = _rand_baoitem(self, titem, tmap);
        if (baoid > 0) {
            me->nbao += 1;
        }
        }
        break;
    }

    struct member* tars[MEMBER_MAX];
    int ntar = _get_effect_members(ro, me, titem->target, tars);
    if (ntar <= 0) {
        return;
    }

    int delay = titem->delay + titem->uptime;
    if (delay > 0) {
        _item_delay(self, ro, me, titem, delay); 
    } else {
        struct member* onetar;
        int i;
        for (i=0; i<ntar; ++i) {
            onetar = tars[i];
            _item_effect_member(self, ro, onetar, titem, 0);
        }
    }

    UM_DEFFIX(UM_ITEMEFFECT, ie);
    ie->spellid = me->detail.charid;
    ie->oriitem = oriitem->id;
    ie->itemid = titem->id;
    struct member* onetar;
    int i;
    for (i=0; i<ntar; ++i) {
        onetar = tars[i];
        ie->charid = onetar->detail.charid; 
        _multicast_msg(ro, (void*)ie, 0);
    }
}

static inline void
notify_create_room_result(struct service *s, int dest_handle, int id, int err) {
    UM_DEFFIX(UM_CREATEROOMRES, result);
    result->id = roomid;
    result->err = err;
    sh_service_send(SERVICE_ID, dest_handle, MT_UM, result, sizeof(*result));
}

static void
_handle_creategame(struct game* self, struct node_message* nm) {
    UM_CAST(UM_CREATEROOM, cr, nm->um);

    const struct map_tplt* tmap = maptplt_find(cr->mapid); 
    if (tmap == NULL) {
        notify_create_room_result(nm->hn, SERR_CRENOTPLT, cr->id, cr->key, 0);
        return;
    }
    struct genmap* gm = _create_map(self, tmap, cr->key);
    if (gm == NULL) {
        notify_create_room_result(nm->hn, SERR_CRENOMAP, cr->id, cr->key, 0);
        return;
    }
    struct room* ro = _create_room(self);
    assert(ro);
    ro->type = cr->type;
    ro->key = cr->key;
    ro->map = gm; 
    ro->gattri.randseed = cr->key; // just easy
    ro->gattri.mapid = cr->mapid;
    ground_attri_build(tmap->difficulty, &ro->gattri);

    ro->np = cr->nmember;
    struct member* m;
    int i;
    for (i=0; i<cr->nmember; ++i) {
        m = &ro->p[i];
        _initmember(m, &cr->members[i]);
        role_attri_build(&ro->gattri, &m->detail.attri);
        //dump(m->detail.charid, m->detail.name, &m->detail.attri);
    }
    int roomid = GFREEID_ID(ro, &self->rooms);
    notify_create_room_result(nm->hn, SERR_OK, cr->id, cr->key, roomid);
}

static void
_handle_destroyroom(struct game* self, struct UM_BASE* um) {
    UM_CAST(UM_CREATEROOMRES, dr, um);
    struct room* ro = _getroom(self, dr->roomid);
    if (ro && (ro->status == RS_CREATE)) {
        _destory_room(self, ro);
    }
}

struct _update_delayud {
    struct game* self;
    struct room* ro;
    struct member* m;
};
static void
_update_delaycb(uint32_t key, void* value, void* ud) {
    uint32_t itemid = key;
    struct _update_delayud* udata = ud;
    struct game* self = udata->self;
    struct room* ro = udata->ro;
    struct member* m = udata->m;
    struct buff_delay* bdelay = value;
    if (bdelay->effect_time > 0) {
        if (bdelay->effect_time <= sc_timer_now()) {
            struct item_tplt* titem = _get_item_tplt(self, itemid);
            if (titem == NULL)
                return;
            int diff = bdelay->last_time > bdelay->effect_time ?
                bdelay->last_time - bdelay->effect_time : 0;
            _item_effect(self, ro, m, titem, diff);

            bdelay->effect_time = 0;
        }
    }
}

static void
_update_buffcb(uint32_t key, void* value, void* ud) {
    struct member* m = ud;
    struct buff* b = value;
    if (b->time > 0 &&
        b->time <= sc_timer_now()/1000) {
        sc_debug("timeout : %u, to char %u", b->time, m->detail.charid);
        b->time = 0;
        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            b->effects[i].value *= -1;
            _item_effectone(m, &b->effects[i]);
        }
    }
}

static void
_update_delay(struct game* self, struct room* ro) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            struct _update_delayud ud1 = {self, ro, m };
            idmap_foreach(m->delaymap, _update_delaycb, &ud1);
        }
    }
}

static void
_update_room(struct game* self, struct room* ro) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            int oxygen = role_oxygen_time_consume(&m->detail.attri);
            if (_reduce_oxygen(m, oxygen) > 0) {
                m->refresh_flag |= REFRESH_ATTRI;
            }
            idmap_foreach(m->buffmap, _update_buffcb, m);
            //bool d = m->refresh_flag & REFRESH_SPEED;
            _on_refresh_attri(m, ro);
            //if (d) {
                //dump(m->detail.charid, m->detail.name, &m->detail.attri);
            //}
        }
    }
}

static void
_handleworld(struct game* self, struct node_message* nm) {
    switch (nm->um->msgid) {
    case IDUM_CREATEROOM:
        _handle_creategame(self, nm);
        break;
    case IDUM_CREATEROOMRES:
        _handle_destroyroom(self, nm->um);
        break;
    }
}
void
game_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct game* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    if (nm.hn->tid == NODE_WORLD) {
        _handleworld(self, &nm);
    }
}

static void
room_create(struct service *s, int source, const struct UM_CREATEROOM *create) {
    struct game* self = SERVICE_SELF;

    struct room *ro = sh_hash_find(&self->rooms, create->id);
    if (ro) {
        notify_create_room_result(s, source, create->id, SERR_ROOMIDCONFLICT);
        return;
    }
    const struct map_tplt *mapt = maptplt_find(create->mapid); 
    if (mapt == NULL) {
        notify_create_room_result(s, source, create->id, SERR_CRENOTPLT);
        return;
    }
    const struct roommap *mapdata = mapdatamgr_find(mapt->id);
    if (mapdata == NULL) {
        notify_create_room_result(s, source, create->id, SERR_CRENOMAP);
        return;
    }
    struct genmap* gm = genmap_create(mapt, m, self->map_randseed++);
    if (gm == NULL) {
        notify_create_room_result(s, source, create->id, SERR_CREMAP);
        return;
    }

    struct room *ro = malloc(sizeof(*ro));
    ro->status = RS_CREATE;
    ro->statustime = sc_timer_now();
    ro->type = create->type;
    ro->map = gm;

    ro->gattri.randseed = cr->key; // just easy
    ro->gattri.mapid = cr->mapid;
    ground_attri_build(mapt->difficulty, &ro->gattri);

    ro->np = cr->nmember;
    struct member* m;
    int i;
    for (i=0; i<cr->nmember; ++i) {
        m = &ro->p[i];
        _initmember(m, &cr->members[i]);
        role_attri_build(&ro->gattri, &m->detail.attri);
        //dump(m->detail.charid, m->detail.name, &m->detail.attri);
    }
    int roomid = GFREEID_ID(ro, &self->rooms);
    notify_create_room_result(nm->hn, SERR_OK, cr->id, cr->key, roomid);

}

static void
room_destroy(struct service *s, uint32_t roomid) {
}

void
game_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_HALL: {
            UM_CAST(UM_HALL, ha, msg);
            UM_CAST(UM_BASE, wrap, ha->wrap);

            switch (wrap->msgid) {
            case IDUM_LOGINROOM:
                //_login(self, gm->c, um);
                break;
            case IDUM_LOGOUT:
                //_logout(self, gm->c, true, true);
                break;
            case IDUM_GAMELOADOK:
                _loadok(self, gm->c);
                break;
            case IDUM_GAMESYNC:
                _sync(self, gm->c, um);
                break;
            case IDUM_ROLEPRESS:
                _role_press(self, gm->c, um);
                break;
            case IDUM_USEITEM:
                _use_item(self, gm->c, um);
                break;
            }
            break;
            }
        case IDUM_CREATEROOM: {
            UM_CAST(UM_CREATEROOM, create, msg);
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

void
game_time(struct service* s) {
    struct game* self = SERVICE_SELF;
    struct room* ro;
    int i;

    if (++self->tick == 10) {
        self->tick = 0;
    }
    for (i=0; i<GFREEID_CAP(&self->rooms); ++i) {
        ro = GFREEID_SLOT(&self->rooms, i);
        if (ro) {
            if (self->tick == 0) {
                switch (ro->status) {
                case RS_CREATE:
                    _check_enter_room(self, ro);
                    break;
                case RS_ENTER:
                    _check_start_room(self, ro);
                    break;
                case RS_START:
                    _update_room(self, ro);
                    _check_over_room(self, ro);
                    break;
                case RS_OVER:
                    _check_destory_room(self, ro);
                    break;
                }
            }
            switch (ro->status) {
            case RS_START:
                _update_delay(self, ro);
                break;
            }
        }
    }
}
