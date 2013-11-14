#include "host_service.h"
#include "host_node.h"
#include "host_timer.h"
#include "host_dispatcher.h"
#include "host_gate.h"
#include "sharetype.h"
#include "node_type.h"
#include "gfreeid.h"
#include "cli_message.h"
#include "user_message.h"
#include "fight.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 1000
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
    int connid; 
    struct tmemberdetail detail;
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
};

struct gfroom {
    GFREEID_FIELDS(room);
};

struct game {
    struct tplt* tpltdata;
    int pmax;
    struct player* players;
    struct gfroom rooms;
};

struct buff_effect {
    int32_t effect;
    int32_t effectvalue;
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

void
game_free(struct game* self) {
    if (self == NULL)
        return;
    free(self->players);
    GFREEID_FINI(room, &self->rooms);
    
    if (self->tpltdata) {
        tplt_fini(self->tpltdata);
        self->tpltdata = NULL;
    }
    free(self);
}

static struct tplt*
_loadtplt(struct game* self) {
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ITEM, sizeof(struct item_tplt), TBLFILE(item), TPLT_VIST_VEC32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
    
}

int
game_init(struct service* s) {
    struct game* self = SERVICE_SELF;
    int pmax = host_gate_maxclient();
    if (pmax == 0) {
        host_error("maxclient is zero, try load service gate before this");
        return 1;
    }
    self->tpltdata = _loadtplt(self);
    if (self->tpltdata == NULL)
        return 1;
    
    self->pmax = pmax;
    self->players = malloc(sizeof(struct player) * pmax);
    memset(self->players, 0, sizeof(struct player) * pmax);
    // todo test this
    GFREEID_INIT(room, &self->rooms, 1);
    SUBSCRIBE_MSG(s->serviceid, IDUM_CREATEROOM);
    SUBSCRIBE_MSG(s->serviceid, IDUM_CREATEROOMRES);

    host_timer_register(s->serviceid, 1000);
    return 0;
}

static inline struct player*
_allocplayer(struct game* self, struct gate_client* c) {
    int id = host_gate_clientid(c);
    assert(id >= 0 && id < self->pmax);
    struct player* p = &self->players[id];
    assert(!p->login);
    return p;
}

static inline struct player*
_getplayer(struct game* self, struct gate_client* c) {
    int id = host_gate_clientid(c);
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
static void
_freebuffcb(uint32_t key, void* value, void* ud) {
    free(value);
}
static void
_freemember(struct member* m) {
    if (m->buffmap) {
        idmap_foreach(m->buffmap, _freebuffcb, NULL);
        idmap_free(m->buffmap);
        m->buffmap = NULL;
    }
}
static void
_verifyfail(struct gate_client* c, int8_t error) {
    UM_DEFFIX(UM_GAMELOGINFAIL, fail);
    fail->error = error;
    UM_SENDTOCLI(c->connid, fail, sizeof(*fail));
}
static inline bool
_elapsed(uint64_t t, uint64_t elapse) {
    uint64_t now = host_timer_now();
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
                    host_debug("disconnect : %u", m->detail.charid);
                    host_gate_disconnclient(c, true);
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
    ro->statustime = host_timer_now();
    return ro;
}
static void
_enter_room(struct room* ro) {
    ro->status = RS_ENTER;
    ro->statustime = host_timer_now();
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
            struct gate_client* c = host_gate_getclient(m->connid);
            if (c) {
                _logout(self, c, true, false);
            }
        }
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
    int n = _count_onlinemember(ro);
    if (n == ro->np) {
        _enter_room(ro);
        return true;
    } else {
        if (_elapsed(ro->statustime, ENTER_TIMEOUT)) {
            if (n > 0) {
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
    const struct member* m1 = p1;
    const struct member* m2 = p2;
    if (m1->deathtime == 0)
        return -1;
    if (m2->deathtime == 0)
        return 1;
    return m1->deathtime < m2->deathtime;
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
    ro->statustime = host_timer_now();
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
        _verifyfail(c, 1);
        host_gate_disconnclient(c, true);
        return;
    }
    if (login->roomkey != ro->key) {
        _verifyfail(c, 2);
        host_gate_disconnclient(c, true);
        return;
    }
    struct member* m = _getmember(ro, login->charid);
    if (m == NULL) {
        _verifyfail(c, 3);
        host_gate_disconnclient(c, true);
        return;
    }
    if (ro->status == RS_OVER) {
        _verifyfail(c, 4);
        host_gate_disconnclient(c, true);
        return;
    }
    struct player* p = _allocplayer(self, c);
    if (p == NULL) {
        _verifyfail(c, 5);
        host_gate_disconnclient(c, true);
        return;
    }
    host_gate_loginclient(c);
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

static int
_item_effectone(struct member* m, int effect, int value, struct buff_effect* result) {
    if (effect == 0 || value == 0)
        return 0;
    host_debug("item effect %d, value %d, to char %u", effect, value, m->detail.charid);
    bool isabs = true;
    if (effect > ITEM_EFFECT_MAX) {
        isabs = false;
        effect -= ITEM_EFFECT_MAX;
    }
    int refresh_flag = 0;
    struct tmemberdetail* detail = &m->detail;
    switch (effect) {
    case ITEM_EFFECT_CREATE_BAO:
        // todo:
        break;
    case ITEM_EFFECT_SPEED:
        if (!isabs) 
            value = detail->quick * value * 0.01;
        _update_value(&detail->quickcur, &value, detail->quick*3);
        refresh_flag |= REFRESH_ROLE;
        break;
    case ITEM_EFFECT_OXYGEN:
        if (!isabs) {
            value = detail->oxygen * value * 0.01;
        }
        _update_value(&detail->oxygencur, &value, detail->oxygen);
        refresh_flag |= REFRESH_ROLE;
        break;
    default:
        break;
    }
    switch (effect) {
    case ITEM_EFFECT_CREATE_BAO:
        m->nbao += 1;
        break;
    case ITEM_EFFECT_OXYGEN:
        m->noxygenitem += 1;
        break;
    default:
        m->nitem += 1;
        break;
    }
    result->effect = effect;
    result->effectvalue = value;
    return refresh_flag;
}

static void
_item_effect_member(struct game* self, struct room* ro, struct member* m, 
        const struct item_tplt* item) {
    int effect_flag = 0;

    struct buff_effect effects[BUFF_EFFECT];
    memset(effects, 0, sizeof(effects));
    if (0 < BUFF_EFFECT) {
        effects[0].effect = item->effect1;
        effects[0].effectvalue = item->effectvalue1;
    }
    if (1 < BUFF_EFFECT) {
        effects[1].effect = item->effect2;
        effects[1].effectvalue = item->effectvalue2;
    }
    if (2 < BUFF_EFFECT) {
        effects[2].effect = item->effect3;
        effects[2].effectvalue = item->effectvalue3;
    }

    int i;
    for (i=0; i<BUFF_EFFECT; ++i) {
        effect_flag |= _item_effectone(m, effects[i].effect, effects[i].effectvalue, &effects[i]);
    }
    if (effect_flag & REFRESH_ROLE) {
        role_attri_build(&ro->gattri, &m->detail);

        UM_DEFFIX(UM_ROLEINFO, ri);
        ri->detail = m->detail;
        _multicast_msg(ro, (void*)ri, 0);
    }

    if (item->time > 0) {
        struct buff* b = idmap_find(m->buffmap, item->id);
        if (b == NULL) {
            b = malloc(sizeof(*b));
            idmap_insert(m->buffmap, item->id, b);
        }
        int i;
        for (i=0; i<BUFF_EFFECT; ++i) {
            b->effects[i] = effects[i];
        }
        b->time = host_timer_now()/1000 + item->time;
        host_debug("insert time: %u, to char %u", b->time, m->detail.charid);
    }

    UM_DEFFIX(UM_ITEMEFFECT, ie);
    ie->charid = m->detail.charid;
    ie->itemid = item->id;
    _multicast_msg(ro, (void*)ie, 0);
}

static void
_use_item(struct game* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p;
    struct room* ro;
    struct member* me;
    if (_locate_player(self, c, &p, &ro, &me))
        return;

    UM_CAST(UM_USEITEM, useitem, um);
    const struct tplt_visitor* vist = tplt_get_visitor(self->tpltdata, TPLT_ITEM);
    if (vist == NULL)
        return;
    const struct item_tplt* item = tplt_visitor_find(vist, useitem->itemid);
    if (item == NULL)
        return;

    host_debug("char %u, use item %u", me->detail.charid, useitem->itemid);
    switch (item->target) {
    case ITEM_TARGET_SELF:
        _item_effect_member(self, ro, me, item);
        break;
    case ITEM_TARGET_ENEMY: {
        struct member* m;
        int i;
        for (i=0; i<ro->np; ++i) {
            m = &ro->p[i];
            if (m->online &&
                m != me) {
                _item_effect_member(self, ro, m, item);
            }
        }
        }
        break;
    case ITEM_TARGET_ALL: {
        struct member* m;
        int i;
        for (i=0; i<ro->np; ++i) {
            m = &ro->p[i];
            if (m->online) {
                _item_effect_member(self, ro, m, item);
            }
        }
        }
        break;
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
_handle_creategame(struct game* self, struct node_message* nm) {
    UM_CAST(UM_CREATEROOM, cr, nm->um);
 
    struct room* ro = _create_room(self);
    assert(ro);
    ro->type = cr->type;
    ro->key = cr->key;
   
    // todo: 
    ground_attri_build(300, &ro->gattri);

    ro->np = cr->nmember;
    int i;
    for (i=0; i<cr->nmember; ++i) {
        _initmember(&ro->p[i], &cr->members[i]);
        role_attri_build(&ro->gattri, &ro->p[i].detail);
    }
    
    UM_DEFFIX(UM_CREATEROOMRES, res);
    res->ok = 1;
    res->id = cr->id;
    res->key = cr->key;
    res->roomid = GFREEID_ID(ro, &self->rooms);
    UM_SENDTONODE(nm->hn, res, sizeof(*res));
}
static void
_handle_destroyroom(struct game* self, struct UM_BASE* um) {
    UM_CAST(UM_CREATEROOMRES, dr, um);
    struct room* ro = _getroom(self, dr->roomid);
    if (ro && (ro->status == RS_CREATE)) {
        _destory_room(self, ro);
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
        b->time <= host_timer_now()/1000) {
        host_debug("timeout : %u, to char %u", b->time, udata->m->detail.charid);
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
_update_room(struct game* self, struct room* ro) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            bool refresh = false;
            int oxygen = -role_oxygen_time_consume(&m->detail);
            //host_debug("char %u, update oxygen %d", m->detail.charid, oxygen);
            _update_value(&m->detail.oxygencur, &oxygen, m->detail.oxygen);
            if (oxygen != 0) {
                refresh = true;
            } 
            struct _update_buffud udata;
            udata.m = m;
            udata.effect_flag = 0;

            idmap_foreach(m->buffmap, _update_buffcb, &udata);
            refresh |= ((udata.effect_flag & REFRESH_ROLE) != 0);
            if (refresh) {
                role_attri_build(&ro->gattri, &m->detail);
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
    for (i=0; i<GFREEID_CAP(&self->rooms); ++i) {
        ro = GFREEID_SLOT(&self->rooms, i);
        if (ro) {
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
    }
}
