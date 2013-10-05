#include "host_service.h"
#include "host.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "freeid.h"
#include "hashid.h"
#include "node_type.h"
#include <stdio.h>
#include <stdlib.h>

#define PS_FREE  0
#define PS_LOGIN 1
#define PS_GAME  2 
struct player {
    uint16_t gid;
    uint16_t cid;
    int status;
    struct chardata data;
};
struct player_holder {
    uint32_t chariditer; // todo
    int cmax;
    int hmax;
    int gmax;
    struct freeid fi;
    struct hashid hi;
    struct player* p;
};

static void
_allocplayers(struct player_holder* ph, int cmax, int hmax, int gmax) {
    if (cmax < 0)
        cmax = 1;
    if (hmax < cmax)
        hmax = cmax;
    if (gmax < 0)
        gmax = 1;
    ph->chariditer = 0;
    ph->cmax = cmax;
    ph->hmax = hmax;
    ph->gmax = gmax;
    ph->p = malloc(sizeof(struct player) * gmax*cmax);
    memset(ph->p, 0, sizeof(struct player) * gmax*cmax);
    freeid_init(&ph->fi, gmax*cmax, gmax*hmax);
    hashid_init(&ph->hi, gmax*cmax, gmax*hmax);
}

#define _isvalidid(gid, cid) \
    ((gid) >= 0 && (gid) < ph->gmax && \
     (cid) >= 0 && (cid) < ph->hmax)
#define _hashid(gid, cid) \
    ((gid)*ph->hmax+(cid))

static struct player*
_getplayer(struct player_holder* ph, uint16_t gid, int cid) {
    if (_isvalidid(gid, cid)) {
        int id = freeid_find(&ph->fi, _hashid(gid, cid));
        if (id >= 0) {
            return &ph->p[id];
        }
    }
    return NULL;
}
/*
static struct player*
_getplayer_byid(struct player_holder* ph, uint32_t charid) {
    return NULL;
}*/
static struct player*
_allocplayer(struct player_holder* ph, uint16_t gid, int cid) {
    if (_isvalidid(gid, cid)) {
        int id = freeid_alloc(&ph->fi, _hashid(gid, cid));
        if (id >= 0) {
            return &ph->p[id];
        }
    }
    return NULL;
}
static void
_freeplayer(struct player_holder* ph, struct player* p) {
    assert(p->status != PS_FREE);
    int hashid = _hashid(p->gid, p->cid);
    int id1 = freeid_free(&ph->fi, hashid);
    assert(id1 >= 0);
    assert(id1 == p-ph->p);
    if (p->data.charid > 0) {
        int id2 = hashid_remove(&ph->hi, p->data.charid);
        assert(id1 == id2);
        p->data.charid = 0;
    }
    p->status = PS_FREE; 
}
struct world {
    struct player_holder ph;
};
struct world*
world_create() {
    struct world* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}
void
world_free(struct world* self) {
    if (self == NULL)
        return;
    freeid_fini(&self->ph.fi);
    hashid_fini(&self->ph.hi);
    free(self);
}
int
world_init(struct service* s) {
    struct world* self = SERVICE_SELF;
    int cmax = host_getint("world_cmax_pergate", 0);
    int hmax = host_getint("world_hmax_pergate", cmax);
    int gmax = host_getint("world_gmax", 0);
    _allocplayers(&self->ph, cmax, hmax, gmax);
    SUBSCRIBE_MSG(s->serviceid, UMID_FORWARD);
    return 0;
}
void
world_service(struct service* s, struct service_message* sm) {
    //struct world* self = SERVICE_SELF;
}
static void
_notify_logout(const struct host_node* node, int cid, int8_t type) {
    UM_FORWARD(fw, cid, UM_logout, lo, UMID_LOGOUT);
    lo->type = LOGOUT_RELOGIN;
    UM_SENDFORWARD(node->connid, fw);
}
static void 
_login_req(struct world* self, const struct host_node* node, int cid, struct UM_base* um) {
    struct player* p;
    p = _getplayer(&self->ph, node->sid, cid);
    if (p != NULL) {
        _notify_logout(node, cid, LOGOUT_RELOGIN);
        return;
    }
    p = _allocplayer(&self->ph, node->sid, cid);
    if (p == NULL) {
        _notify_logout(node, cid, LOGOUT_FULL);
        return;
    }
    p->status = PS_LOGIN;
    uint32_t charid = ++self->ph.chariditer;
    int id = hashid_hash(&self->ph.hi, charid);
    if (id == -1) {
        _notify_logout(node, cid, LOGOUT_FULL);
        _freeplayer(&self->ph, p);
        return;
    }
    assert(id == p-self->ph.p);
    // todo: this just for test
    struct chardata* data = &p->data;
    data->charid = charid;
    snprintf(data->name, sizeof(data->name), "wabao-n%u", charid);
    
    UM_FORWARD(fw, cid, UM_charinfo, ci, UMID_CHARINFO);
    ci->data = *data;
    UM_SENDFORWARD(node->connid, fw);
}
static void
_logout_req(struct world* self, struct player* p) {
    _freeplayer(&self->ph, p);
}
static void
_play_req(struct world* self, struct player* p) {
}
static void 
_handlegate(struct world* self, const struct host_node* node, struct UM_base* um) {
    assert(um->msgid == UMID_FORWARD);
    UM_CAST(UM_forward, fw, um);
    struct UM_base* m = &fw->wrap;
    if (m->msgid == UMID_LOGIN) {
        _login_req(self, node, fw->cid, m);
        return;
    } else {
        struct player* p = _getplayer(&self->ph, node->sid, fw->cid);
        if (p == NULL) {
            _notify_logout(node, fw->cid, LOGOUT_NOLOGIN);
            return;
        }
        switch (m->msgid) {
        case UMID_LOGOUT:
            _logout_req(self, p);
            break;
        case UMID_PLAY:
            _play_req(self, p);
            break;
        }
    }
}
static void
_handlegame(struct world* self, const struct host_node* node, struct UM_base* um) {
}
void
world_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct world* self = SERVICE_SELF;
    UM_CAST(UM_base, um, msg);

    const struct host_node* node = host_node_get(um->nodeid);
    if (node == NULL || node->connid != id) {
        host_error("dismatch node %u, from msg %u", um->nodeid, um->msgid);
        return;
    }
    switch (node->tid) {
    case NODE_GATE:
        _handlegate(self, node, um);
        break;
    case NODE_GAME:
        _handlegame(self, node, um);
        break;
    }
}
void
world_time(struct service* s) {
    //struct world* self= SERVICE_SELF;
}
