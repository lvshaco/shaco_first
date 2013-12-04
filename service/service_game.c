#include "sc_service.h"
#include "sc_util.h"
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 3000
#define DESTROY_TIMEOUT 500

#define RS_CREATE 0
#define RS_ENTER  1
#define RS_START  2
#define RS_OVER   3

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
    struct tmemberdetail detail;
    struct idmap* delaymap;
    struct idmap* buffmap;
    int32_t depth;
    uint64_t deathtime;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t nbao;
};

struct room { 
    int id;
    int used;
    int8_t type;
    uint32_t key;
    int status; // RS_*
    uint64_t statustime;
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
    int pmax;
    struct player* players;
    struct gfroom rooms;
    int tick;
    uint32_t randseed;
};

struct buff_delay {
    uint64_t effect_time;
    uint64_t last_time;
};

struct buff_effect {
    int32_t effect;
    float effectvalue;
};

#define BUFF_EFFECT 3

struct buff {
    struct buff_effect effects[BUFF_EFFECT];
    int time;
};

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
    self->tplt_handler = service_query_id("tpltgame");
    if (self->tplt_handler == SERVICE_INVALID) {
        sc_error("lost tpltgame service");
        return 1;
    }
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

    self->randseed = time(NULL);

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
    m->detail.oxygencur = m->detail.oxygen;
    m->detail.bodycur = m->detail.body;
    m->detail.quickcur = m->detail.quick;
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
    return m1->detail.oxygencur >= m2->detail.oxygencur ? -1 : 1;
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
            if (m->detail.oxygencur == 0) {
                isgameover = true;
                break;
            }
        }
    }
    if (!isgameover)
        return;

    // rank
    struct member* pmarray[ro->np];
    for (i=0; i<ro->np; ++i) {
        pmarray[i] = &ro->p[i];
    }
    qsort(pmarray, ro->np, sizeof(pmarray[0]), _rankcmp);

    UM_DEFVAR(UM_GAMEOVER, go);
    go->nmember = ro->np;
    for (i=0; i<ro->np; ++i) {
        m = pmarray[i];
        go->stats[i].charid = m->detail.charid;
        go->stats[i].depth = m->depth;
        go->stats[i].noxygenitem = m->noxygenitem;
        go->stats[i].nitem = m->nitem;
        go->stats[i].nbao = m->nbao;
    }
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            UM_SENDTOCLI(m->connid, go, UM_GAMEOVER_size(go));
        }
    }
    ro->status = RS_OVER;
    ro->statustime = sc_timer_now();
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
_maptplt(uint32_t mapid) {
    const struct tplt_visitor* visitor = tplt_get_visitor(TPLT_MAP);
    if (visitor) 
        return tplt_visitor_find(visitor, mapid);
    return NULL; 
}

static struct genmap*
_create_map(struct game* self, const struct map_tplt* tplt, uint32_t seed) {
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
}

static void
_role_press(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p;
    struct room* ro;
    struct member* m;
    if (_locate_player(self, c, &p, &ro, &m))
        return;
    UM_CAST(UM_GAMEPRESS, gp, um);
    _multicast_msg(ro, (void*)gp, p->charid);
}

// refresh data type, binary bit
#define REFRESH_ROLE 1

static inline void
_update_value(int* cur, int* value, int max) {
    int old = *cur;
    *cur += *value;
    if (*cur < 0) {
        *cur = 0;
        *value = -old; 
    } else if (*cur > max) {
        *cur = max;
        *value = max - old;
    }
}
static inline void
_update_valuef(float* cur, float* value, float max) {
    float old = *cur;
    *cur += *value;
    if (*cur < 0) {
        *cur = 0;
        *value = -old; 
    } else if (*cur > max) {
        *cur = max;
        *value = max - old;
    }
}


static int
_item_effectone(struct member* m, int effect, float value, struct buff_effect* result) {
    if (effect == 0 || value == 0)
        return 0;
    sc_debug("titem effect %d, value %f, to char %u", effect, value, m->detail.charid);
    bool isabs = true;
    if (effect > ITEM_EFFECT_MAX) {
        isabs = false;
        effect -= ITEM_EFFECT_MAX;
    }
    int refresh_flag = 0;
    struct tmemberdetail* detail = &m->detail;
    switch (effect) {
    case ITEM_EFFECT_SPEED:
        if (!isabs) 
            value = detail->movespeed * value * 0.01;
        sc_debug("char %u, speed before %f, value %f", detail->charid, detail->movespeed, value);
        _update_valuef(&detail->movespeed, &value, detail->movespeed*3);
        sc_debug("char %u, speed after %f, value %f", detail->charid, detail->movespeed, value);

        refresh_flag |= REFRESH_ROLE;
        break;
    case ITEM_EFFECT_OXYGEN: {
        int ivalue = value;
        if (!isabs) {
            ivalue = detail->oxygen * ivalue * 0.01;
        }
        _update_value(&detail->oxygencur, &ivalue, detail->oxygen);
        refresh_flag |= REFRESH_ROLE;
        }
        break;
    default:
        break;
    }
    result->effect = effect;
    result->effectvalue = value;
    return refresh_flag;
}

static void
_item_effect_member(struct game* self, struct room* ro, struct member* m, 
        const struct item_tplt* titem, int addtime) {

    struct buff* b = NULL;

    bool spell = false;
    if (titem->time == 0) {
        spell = true;
    } else {
        b = idmap_find(m->buffmap, titem->id);
        if (b == NULL) {
            b = malloc(sizeof(*b));
            idmap_insert(m->buffmap, titem->id, b);
            spell = true;
        } else {
            if (b->time == 0) {
                spell = true;
            }
        }
    }
    struct buff_effect effects[BUFF_EFFECT];
    if (spell) {
        int effect_flag = 0;
        memset(effects, 0, sizeof(effects));
        if (0 < BUFF_EFFECT) {
            effects[0].effect = titem->effect1;
            effects[0].effectvalue = titem->effectvalue1;
        }
        if (1 < BUFF_EFFECT) {
            effects[1].effect = titem->effect2;
            effects[1].effectvalue = titem->effectvalue2;
        }
        if (2 < BUFF_EFFECT) {
            effects[2].effect = titem->effect3;
            effects[2].effectvalue = titem->effectvalue3;
        }

        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            effect_flag |= _item_effectone(m, effects[i].effect, effects[i].effectvalue, &effects[i]);
        }
        if (effect_flag & REFRESH_ROLE) {
            //role_attri_build(&ro->gattri, &m->detail);
            sc_debug("char %u, speed %f", m->detail.charid, m->detail.movespeed);
            UM_DEFFIX(UM_ROLEINFO, ri);
            ri->detail = m->detail;
            _multicast_msg(ro, (void*)ri, 0);
        }
    }
    if (b) {
        if (spell) {
            int i;
            for (i=0; i<BUFF_EFFECT; ++i) {
                b->effects[i] = effects[i];
            }
        }
        b->time = sc_timer_now()/1000 + titem->time + addtime;
        sc_debug("insert time: %u, to char %u", b->time, m->detail.charid);
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
    const struct map_tplt* tmap = _maptplt(ro->gattri.mapid); 
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

    int delay = titem->delay;
    if (delay > 0 && oriitem != titem) {
        delay += oriitem->delay;
    }
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

void
game_usermsg(struct service* s, int id, void* msg, int sz) {
    struct game* self = SERVICE_SELF;
    struct gate_message* gm = msg;
    assert(gm->c);
    UM_CAST(UM_BASE, um, gm->msg);
    switch (um->msgid) {
    case IDUM_GAMELOGIN:
        _login(self, gm->c, um);
        break;
    case IDUM_GAMELOGOUT:
        _logout(self, gm->c, true, true);
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
}

static void
_notify_createroomres(const struct sc_node* node, 
        int error, int id, uint32_t key, int roomid) {
    UM_DEFFIX(UM_CREATEROOMRES, res);
    res->error = error;
    res->id = id;
    res->key = key;
    res->roomid = roomid;
    UM_SENDTONODE(node, res, sizeof(*res));
}

static void
_handle_creategame(struct game* self, struct node_message* nm) {
    UM_CAST(UM_CREATEROOM, cr, nm->um);

    const struct map_tplt* tmap = _maptplt(cr->mapid); 
    if (tmap == NULL) {
        _notify_createroomres(nm->hn, SERR_CRENOTPLT, cr->id, cr->key, 0);
        return;
    }
    struct genmap* m = _create_map(self, tmap, cr->key);
    if (m == NULL) {
        _notify_createroomres(nm->hn, SERR_CRENOMAP, cr->id, cr->key, 0);
        return;
    }
    struct room* ro = _create_room(self);
    assert(ro);
    ro->type = cr->type;
    ro->key = cr->key;
    ro->map = m; 
    ro->gattri.randseed = cr->key; // just easy
    ro->gattri.mapid = cr->mapid;
    ground_attri_build(tmap->difficulty, &ro->gattri);

    ro->np = cr->nmember;
    int i;
    for (i=0; i<cr->nmember; ++i) {
        _initmember(&ro->p[i], &cr->members[i]);
        role_attri_build(&ro->gattri, &ro->p[i].detail);
    }
    int roomid = GFREEID_ID(ro, &self->rooms);
    _notify_createroomres(nm->hn, SERR_OK, cr->id, cr->key, roomid);
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

struct _update_buffud {
    struct member* m;
    int effect_flag;
};
static void
_update_buffcb(uint32_t key, void* value, void* ud) {
    struct _update_buffud* udata = ud;
    struct buff* b = value;
    if (b->time > 0 &&
        b->time <= sc_timer_now()/1000) {
        sc_debug("timeout : %u, to char %u", b->time, udata->m->detail.charid);
        b->time = 0;
        struct buff_effect effect;
        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            udata->effect_flag |= _item_effectone(udata->m, 
                    b->effects[i].effect,
                    -b->effects[i].effectvalue,
                    &effect);
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
            bool refresh = false;
            int oxygen = -role_oxygen_time_consume(&m->detail);
            //sc_debug("char %u, update oxygen %d", m->detail.charid, oxygen);
            _update_value(&m->detail.oxygencur, &oxygen, m->detail.oxygen);
            if (oxygen != 0) {
                refresh = true;
            } 

            struct _update_buffud ud2 = {m, 0};
            idmap_foreach(m->buffmap, _update_buffcb, &ud2);

            refresh |= ((ud2.effect_flag & REFRESH_ROLE) != 0);
            if (refresh) {
                //role_attri_build(&ro->gattri, &m->detail);
                UM_DEFFIX(UM_ROLEINFO, ri);
                ri->detail = m->detail;
                _multicast_msg(ro, (void*)ri, 0);
            }
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

void
game_net(struct service* s, struct gate_message* gm) {
    struct game* self = SERVICE_SELF;
    struct net_message* nm = gm->msg;
    switch (nm->type) {
    case NETE_SOCKERR:
    case NETE_TIMEOUT:
        _logout(self, gm->c, false, true);
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
