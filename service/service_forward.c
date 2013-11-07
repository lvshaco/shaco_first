#include "host_service.h"
#include "host_timer.h"
#include "host_gate.h"
#include "host_node.h"
#include "host_dispatcher.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct accinfo {
    uint64_t regtime;
    uint32_t accid;
    uint64_t key;
    uint32_t clientip;
    char account[ACCOUNT_NAME_MAX];
};

#define STATE_FREE 0
#define STATE_LOGIN 1
struct player {
    int state;
};

struct forward {
    int pmax;
    struct player* players;
    struct idmap* regacc;
};

struct forward*
forward_create() {
    struct forward* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

static void
_freecb(uint32_t key, void* value, void* ud) {
    free(value);
}
void
forward_free(struct forward* self) {
    if (self == NULL)
        return;
    if (self->players) {
        free(self->players);
    }
    if (self->regacc) {
        idmap_foreach(self->regacc, _freecb, NULL);
        idmap_free(self->regacc);
    }
    free(self);
}

int
forward_init(struct service* s) {
    struct forward* self = SERVICE_SELF;
    int cmax = host_gate_maxclient();
    self->players = malloc(sizeof(struct player) * cmax);
    memset(self->players, 0, sizeof(struct player) * cmax);
    self->pmax = cmax;
    self->regacc = idmap_create(cmax); // memory

    SUBSCRIBE_MSG(s->serviceid, IDUM_FORWARD);
    host_timer_register(s->serviceid, 1000);
    return 0;
}

struct player* 
_getplayer(struct forward* self, struct gate_client* c) {
    int id = host_gate_clientid(c);
    assert(id >= 0 && id < self->pmax);
    return &self->players[id];
}

static inline void
_forward_world(struct gate_client* c, struct UM_BASE* um) {
    UM_DEFVAR(UM_FORWARD, fw);
    fw->cid = c->connid;
    memcpy(&fw->wrap, um, um->msgsz);
    fw->wrap.nodeid = host_id();
    const struct host_node* node = host_node_get(HNODE_ID(NODE_WORLD, 0));
    if (node) {
        UM_SEND(node->connid, fw, UM_FORWARD_size(fw));
    }
}

static inline void
_updateload() {
    const struct host_node* node = host_node_get(HNODE_ID(NODE_LOAD, 0));
    if (node) {
        UM_DEFFIX(UM_UPDATELOAD, load);
        load->value = host_gate_usedclient();
        UM_SENDTONODE(node, load, load->msgsz);
    }
}

static inline void
_onaccept() {
    _updateload();
}

static inline void
_ondisconn(struct forward*self, struct gate_client* c) {
    struct player* p = _getplayer(self, c);
    p->state = STATE_FREE;
    _updateload();
}

static int
_login(struct forward* self, struct gate_client* c, struct UM_BASE* um) {
    struct player* p = _getplayer(self, c);
    assert(p);
    if (p->state == STATE_LOGIN) {
        return 1;
    }
    UM_CAST(UM_LOGIN, login, um);
    struct accinfo* acc = idmap_remove(self->regacc, login->accid);
    if (acc == NULL) {
        host_gate_disconnclient(c, true);
        _ondisconn(self, c);
        return 1;
    }
    login->account[sizeof(login->account)-1] = '\0';
    uint32_t addr;
    uint16_t port;
    host_net_socket_address(c->connid, &addr, &port);
    if (acc->key == login->key &&
        acc->clientip == addr &&
        strcmp(acc->account, login->account) == 0) {
        free(acc);
        p->state = STATE_LOGIN;
        return 0;
    } else {
        free(acc);
        return 1;
    }
}

void
forward_usermsg(struct service* s, int id, void* msg, int sz) {
    struct forward* self = SERVICE_SELF;
    struct gate_message* gm = msg;
    struct gate_client* c = gm->c;
    UM_CAST(UM_BASE, um, gm->msg);
    if (um->msgid >= IDUM_CBEGIN &&
        um->msgid <  IDUM_CEND) {
        if (um->msgid == IDUM_LOGIN) {
            if (_login(self, c, um)) {
                return;
            }
        }
        _forward_world(c, um);
    } else {
        // todo: just disconnect it ?
        host_gate_disconnclient(c, true); 
        _ondisconn(self, c);
    }
}

static void
_accountreg(struct forward* self, int fid, const struct host_node* source, struct UM_BASE* um) {
    UM_CAST(UM_ACCOUNTLOGINREG, reg, um);
    struct accinfo* acc = idmap_find(self->regacc, reg->accid);
    if (acc == NULL) {
        acc = malloc(sizeof(*acc));
        acc->regtime = host_timer_now();
        acc->accid = reg->accid;
        acc->key = reg->key;
        acc->clientip = reg->clientip;
        strncpy(acc->account, reg->account, ACCOUNT_NAME_MAX);
        idmap_insert(self->regacc, reg->accid, acc);
    } else {
        // update (wait for the key timeout is not good!)
        acc->regtime = host_timer_now();
        acc->key = reg->key;
        acc->clientip = reg->clientip;
        strncpy(acc->account, reg->account, ACCOUNT_NAME_MAX);
    }
    const struct host_node* me = host_me();
    UM_DEFFORWARD(fw, fid, UM_ACCOUNTLOGINRES, res);
    res->ok = 1;
    res->cid = reg->cid;
    res->accid = reg->accid;
    res->key = reg->key;
    res->addr = me->gaddr;
    res->port = me->gport;
    UM_SENDFORWARD(source->connid, fw);
}

static void
_handleload(struct forward* self, struct node_message* nm) {
    if (nm->um->msgid != IDUM_FORWARD) {
        return;
    }
    UM_CAST(UM_FORWARD, fw, nm->um);
    struct UM_BASE* m = &fw->wrap;
    switch (m->msgid) {
    case IDUM_ACCOUNTLOGINREG:
        _accountreg(self, fw->cid, nm->hn, m);
        break;
    }
}

static void
_handledef(struct forward*self, struct node_message* nm) {
    if (nm->um->msgid != IDUM_FORWARD) {
        return;
    }
    UM_CAST(UM_FORWARD, fw, nm->um);
    struct UM_BASE* m = &fw->wrap;
    struct gate_client* c = host_gate_getclient(fw->cid);
    if (c) {
        host_debug("Send msg:%u",  m->msgid);
        if (m->msgid == IDUM_LOGOUT) {
            UM_SENDTOCLI(c->connid, m, m->msgsz);
            host_gate_disconnclient(c, true);
            _ondisconn(self, c);
        } else {
            UM_SENDTOCLI(c->connid, m, m->msgsz);
        }
    }
}

void
forward_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct forward* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_LOAD:
        _handleload(self, &nm);
        break;
    default:
        _handledef(self, &nm);
        break;
    }
}

void
forward_net(struct service* s, struct gate_message* gm) {
    struct forward* self = SERVICE_SELF;
    struct net_message* nm = gm->msg;
    UM_DEFFIX(UM_LOGOUT, logout);
    switch (nm->type) {
    case NETE_ACCEPT:
        _onaccept();
        break;
    case NETE_SOCKERR:
        logout->error = SERR_SOCKET;
        _forward_world(gm->c, (struct UM_BASE*)logout);
        _ondisconn(self, gm->c);
        break;
    case NETE_TIMEOUT:
        logout->error = SERR_TIMEOUT;
        _forward_world(gm->c, (struct UM_BASE*)logout);
        _ondisconn(self, gm->c);
        break;
    }
}

static void
_acctimecb(uint32_t key, void* value, void* ud) {
    struct forward* self = ud;
    struct accinfo* acc = value;
    uint64_t now = host_timer_now();
    if (now > acc->regtime &&
        now - acc->regtime > 10*1000) {
        // todo: optimize
        void* rm = idmap_remove(self->regacc, key);
        free(rm);
    }
}

void
forward_time(struct service* s) {
    struct forward* self= SERVICE_SELF;
    idmap_foreach(self->regacc, _acctimecb, self);
}
