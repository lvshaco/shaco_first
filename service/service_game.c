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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ENTER_TIMELEAST (ROOM_LOAD_TIMELEAST*1000)
#define ENTER_TIMEOUT (5000+ENTER_TIMELEAST)
#define START_TIMEOUT 1000

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
};

struct room { 
    int id;
    int used;
    uint64_t createtime; 
    uint64_t entertime;
    int8_t type;
    uint32_t key;
    int status; // RS_*
    int np;
    struct member p[MEMBER_MAX];
};

struct gfroom {
    GFREEID_FIELDS(room);
};

struct game {
    int pmax;
    struct player* players;
    struct gfroom rooms;
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
    free(self);
}

int
game_init(struct service* s) {
    struct game* self = SERVICE_SELF;
    int pmax = host_gate_maxclient();
    if (pmax == 0) {
        host_error("maxclient is zero, try load service gate before this");
        return 1;
    }
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

static struct player*
_getplayer(struct game* self, struct gate_client* c) {
    int id = host_gate_clientid(c);
    assert(id >= 0 && id < self->pmax);
    return &self->players[id];
}

static struct room*
_getroom(struct game* self, int roomid) {
    struct room* ro = GFREEID_SLOT(&self->rooms, roomid);
    return ro;
}

static inline void
_initmember(struct member* m, struct tmemberdetail* detail) {
    m->detail = *detail;
    m->login = false;
    m->online = false;
    m->connid = -1;
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
_multicast_msg(struct room* ro, struct UM_BASE* um) {
    struct member* m;
    int i;
    for (i=0; i<ro->np; ++i) {
        m = &ro->p[i];
        if (m->online) {
            UM_SENDTOCLIDIRECT(m->connid, um);
        }
    }
}
static struct room*
_gamecreate(struct game* self) {
    struct room* ro = GFREEID_ALLOC(room, &self->rooms);
    assert(ro);
    ro->createtime = host_timer_now();
    ro->status = RS_CREATE;
    return ro;
}
static void
_gameenter(struct room* ro) {
    ro->status = RS_ENTER;
    ro->entertime = host_timer_now();
    UM_DEFFIX(UM_GAMEENTER, enter);
    enter->msgsz -= UM_SKIP;
    _multicast_msg(ro, (void*)enter);
}
static void
_gamestart(struct room* ro) {
    ro->status = RS_START;
    UM_DEFFIX(UM_GAMESTART, start);
    start->msgsz -= UM_SKIP;
    _multicast_msg(ro, (void*)start);
}
static void
_gamedestroy(struct game* self, struct room* ro) {
    GFREEID_FREE(room, &self->rooms, ro);
}
static bool
_try_gameenter(struct game* self, struct room* ro) {
    if (ro->status != RS_CREATE) {
        return false;
    }
    if (!_elapsed(ro->createtime, ENTER_TIMELEAST)) {
        return false;
    }
    int n = _count_onlinemember(ro);
    if (n == ro->np) {
        _gameenter(ro);
        return true;
    } else {
        if (_elapsed(ro->createtime, ENTER_TIMEOUT)) {
            if (n > 0) {
                _gameenter(ro);
                return true;
            } else {
                _gamedestroy(self, ro);
                return false;
            }
        }
    }
    return false;
}
static bool
_try_gamestart(struct game* self, struct room* ro) {
    if (ro->status != RS_ENTER) {
        return false;
    }
    if (_elapsed(ro->entertime, START_TIMEOUT)) {
        _gamestart(ro);
        return true;
    }
    return false;
}
static bool
_try_gameover(struct game* self, struct room* ro) {
    if (ro->status != RS_START) {
        return false;
    }
    int n = _count_onlinemember(ro);
    if (n == 0) {
        _gamedestroy(self, ro);
    }
    return true;
}
static void
_notify_gameinfo(struct gate_client* c, struct room* ro) {
    UM_DEFVAR(UM_GAMEINFO, ri);
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
        return;
    }
    if (login->roomkey != ro->key) {
        _verifyfail(c, 2);
        return;
    }
    struct member* m = _getmember(ro, login->charid);
    if (m == NULL) {
        _verifyfail(c, 3);
        return;
    }
    if (ro->status == RS_OVER) {
        _verifyfail(c, 4);
        return;
    }
    struct player* p = _getplayer(self, c);
    if (p == NULL) {
        _verifyfail(c, 5);
        return;
    }
    p->login = true;
    p->roomid = GFREEID_ID(ro, &self->rooms);
    p->charid = m->detail.charid;
    m->login = 1;
    m->online = 1;
    m->connid = c->connid;

    _notify_gameinfo(c, ro);
    _try_gameenter(self, ro);
    return;
}
static void
_logout(struct game* self, struct gate_client* c, bool active) {
    struct player* p = _getplayer(self, c);
    if (p == NULL) {
        return;
    }
    if (p->login) {
        int roomid = p->roomid;
        p->login = false;
        p->roomid = 0;
        p->charid = 0;
        struct room* ro = _getroom(self, roomid);
        if (ro) {
            struct member* m = _getmember(ro, p->charid);
            if (m) {
                m->online = false;
                if (active) {
                    _try_gameover(self, ro);
                }
            }
        }
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
        _logout(self, gm->c, true);
        break;
    }
}

static void
_creategame(struct game* self, struct node_message* nm) {
    UM_CAST(UM_CREATEROOM, cr, nm->um);

    struct room* ro = _gamecreate(self);
    assert(ro);
    ro->type = cr->type;
    ro->key = cr->key;
    ro->np = cr->nmember;
    int i;
    for (i=0; i<cr->nmember; ++i) {
        _initmember(&ro->p[i], &cr->members[i]);
    }
    UM_DEFFIX(UM_CREATEROOMRES, res);
    res->ok = 1;
    res->id = cr->id;
    res->key = cr->key;
    res->roomid = GFREEID_ID(ro, &self->rooms);
    UM_SENDTONODE(nm->hn, res, sizeof(*res));
}
static void
_destroyroom(struct game* self, struct UM_BASE* um) {
    UM_CAST(UM_CREATEROOMRES, dr, um);
    struct room* ro = _getroom(self, dr->roomid);
    if (ro && (ro->status == RS_CREATE)) {
        _gamedestroy(self, ro);
    }
}
static void
_handleworld(struct game* self, struct node_message* nm) {
    switch (nm->um->msgid) {
    case IDUM_CREATEROOM:
        _creategame(self, nm);
        break;
    case IDUM_CREATEROOMRES:
        _destroyroom(self, nm->um);
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
        _logout(self, gm->c, false);
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
                _try_gameenter(self, ro);
                break;
            case RS_ENTER:
                _try_gamestart(self, ro);
                break;
            }
        }
    }
}
