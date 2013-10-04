#include "host_service.h"
#include "host.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_net.h"
#include "host_dispatcher.h"
#include "node_type.h"
#include "client_type.h"
#include "user_message.h"
#include "freeid.h"
#include "args.h"
#include "memrw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct client {
    int connid;
    uint64_t active_time;
    bool connected;
};

struct server {
    int ctl_service;
    int max;
    int livetime;
    struct freeid fi;
    struct client* clients;
};

struct server*
cmds_create() {
    struct server* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
cmds_free(struct server* self) {
    freeid_fini(&self->fi);
    free(self->clients);
    free(self);
}

static int
_listen(struct service* s) {
    const char* addr = host_getstr("cmds_ip", "");
    int port = host_getint("cmds_port", 0);
    if (addr[0] != '\0' &&
        host_net_listen(addr, port, s->serviceid, CLI_CMD)) {
        host_error("listen cmds fail");
        return 1;
    }
    return 0;
}

int
cmds_init(struct service* s) {
    struct server* self = SERVICE_SELF;
    if (_listen(s))
        return 1;

    self->ctl_service = service_query_id("cmdctl");
    if (self->ctl_service == -1) {
        host_error("lost cmdctl service");
        return 1;
    }

    int cmax = host_getint("cmdc_max", 10);
    int live = host_getint("cmdc_livetime", 60);
    int hmax = host_getint("host_connmax", cmax); 
    self->livetime = live * 1000;
    self->max = cmax;
    self->clients = malloc(sizeof(struct client) * cmax);
    memset(self->clients, 0, sizeof(struct client) * cmax);
    freeid_init(&self->fi, cmax, hmax);
    
    host_timer_register(s->serviceid, self->livetime);

    SUBSCRIBE_MSG(s->serviceid, UMID_CMD_RES);
    return 0;
}

static inline struct client*
_getclient(struct server* self, int id) {
    if (id >= 0 && id < self->max) {
        struct client* c = &self->clients[id];
        if (c->connected)
            return c;
    }
    return NULL;
}

static void
_response_error(int id, const char* error) {
    UM_DEF(um, 1024);
    int n = snprintf((char*)(um+1), 1024-UM_HSIZE, "%s", error);
    UM_SENDTOCLI(id, um, UM_HSIZE+n);
}

static inline int
_sendto_remote(const struct host_node* node, void* ud) {
    struct UM_base* um = ud;
    UM_SEND(node->connid, um, um->msgsz);
    return 0;
}

static void
_routeto_node(struct server* self, uint16_t nodeid, struct UM_base* um) {
    if (nodeid == host_id()) {
        service_notify_nodemsg(self->ctl_service, -1, um, um->msgsz);
    } else {
        const struct host_node* node = host_node_get(nodeid);
        if (node) {
            _sendto_remote(node, um);
        }
    }
}

static void
_broadcast_type(struct server* self, int tid, struct UM_base* um) {
    if (tid == NODE_CENTER) {
        _routeto_node(self, host_id(), um); 
    } else {
        host_node_foreach(tid, _sendto_remote, um);
    }
}

static void
_broadcast_all(struct server* self, struct UM_base* um) {
    int i;
    for (i=0; i<host_node_types(); ++i) {
        _broadcast_type(self, i, um);
    } 
}

static int
_getnodeid(const char* tidstr, const char* sidstr) {
    if (strcasecmp(tidstr, "all") == 0) {
        return HNODE_ID(HNODE_TID_MAX, HNODE_SID_MAX);
    }
    int tid = host_node_typeid(tidstr);
    if (tid == -1) {
        return -1;
    }
    if (strcasecmp(sidstr, "all") == 0) {
        return HNODE_ID(tid, HNODE_SID_MAX);
    } else {
        return HNODE_ID(tid, atoi(sidstr));
    }
}

void
cmds_usermsg(struct service* s, int id, void* msg, int sz) {
    struct server* self = SERVICE_SELF;
    int hash = freeid_find(&self->fi, id);
    struct client* c = _getclient(self, hash);
    assert(c);
    UM_CAST(UM_base, um, msg);
   
    struct args A;
    args_parsestrl(&A, 3, (char*)(um+1), sz-UM_HSIZE);
    if (A.argc < 3) {
        _response_error(id, "usage: node sid command [arg1 arg2 .. ]");
        return;
    }
    int nodeid = _getnodeid(A.argv[0], A.argv[1]);
    if (nodeid == -1) {
        _response_error(id, "invalid node");
        return;
    }
    int tid = HNODE_TID(nodeid);
    int sid = HNODE_SID(nodeid);

    UM_DEFVAR(UM_cmd_req, req, UMID_CMD_REQ);
    req->cid = hash;
    size_t l = strlen(A.argv[2]);
    memcpy(req->cmd, A.argv[2], l);
    req->msgsz = sizeof(*req) + l;

    um = (struct UM_base*)req;
    if (tid == HNODE_TID_MAX) {
        _broadcast_all(self, um); 
    } else if (sid == HNODE_SID_MAX) {
        _broadcast_type(self, tid, um);
    } else {
        _routeto_node(self, HNODE_ID(tid, sid), um);
    }
}

static void
_res(struct server* self, int id, struct UM_base* um) {
    const struct host_node* node = host_node_get(um->nodeid);
    if (node == NULL)
        return;
    UM_CAST(UM_cmd_res, res, um);
    struct client* c = _getclient(self, res->cid);
    if (c == NULL)
        return;
    UM_DEF(notify, UM_MAXSIZE);
    struct memrw rw;
    memrw_init(&rw, notify+1, UM_MAXSIZE-sizeof(*notify));
    char tmp[HNODESTR_MAX]; 
    int n = snprintf(rw.ptr, RW_SPACE(&rw), "%s:\n", 
            host_strnode(node, tmp));
    memrw_pos(&rw, n);
    size_t sz = res->msgsz - sizeof(*res); 
    memrw_write(&rw, res+1, sz);
    notify->msgsz = sizeof(*notify) + RW_CUR(&rw);
    UM_SENDTOCLI(c->connid, notify, notify->msgsz);
}

void
cmds_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct server* self = SERVICE_SELF;
    UM_CAST(UM_base, um, msg);
    switch (um->msgid) {
    case UMID_CMD_RES:
        _res(self, id, um);
        break;
    }
}

static void
_accept(struct server* self, int connid) {
    int id = freeid_alloc(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    assert(!c->connected);
    c->connected = true;
    c->connid = connid;
    c->active_time = host_timer_now();
    
    host_net_subscribe(connid, true, false);
}

static void
_ondisconnect(struct server* self, int connid) {
    int id = freeid_free(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    c->connected = false;
}

static void
_disconnect(struct server* self, int id, const char* error) {
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id]; 
    assert(c->connected);
    assert(c->connid >= 0);
    _response_error(c->connid, error);
    int tmp = freeid_free(&self->fi, c->connid);
    assert(tmp == id);
    host_net_close_socket(c->connid);
    c->connected = false;
}

void
cmds_net(struct service* s, struct net_message* nm) {
    struct server* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_ACCEPT:
        _accept(self, nm->connid);
        break;
    case NETE_SOCKERR:
        _ondisconnect(self, nm->connid);
        break;
    }
}

void
cmds_time(struct service* s) {
    
    struct server* self= SERVICE_SELF;
    struct client* clients = self->clients;
    struct client* c;
    uint64_t now = host_timer_now();
    int i;
    for (i=0; i<self->max; ++i) {
        c = &clients[i];
        if (c->connected) {
            if (now > c->active_time &&
                now - c->active_time > self->livetime) { 
                _disconnect(self, i, "livetimeout.");
            }
        }
    }
}
