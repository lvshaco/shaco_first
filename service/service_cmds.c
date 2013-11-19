#include "sc_service.h"
#include "sc.h"
#include "sc_log.h"
#include "sc_dispatcher.h"
#include "sc_gate.h"
#include "node_type.h"
#include "user_message.h"
#include "memrw.h"
#include "args.h"
#include <stdio.h>
#include <assert.h>

struct server {
    int ctl_service;
};

struct server*
cmds_create() {
    struct server* self = malloc(sizeof(*self));
    self->ctl_service = SERVICE_INVALID;
    return self;
}

void
cmds_free(struct server* self) {
    free(self);
}

int
cmds_init(struct service* s) {
    struct server* self = SERVICE_SELF;
    self->ctl_service = service_query_id("cmdctl");
    if (self->ctl_service == SERVICE_INVALID) {
        sc_error("lost cmdctl service");
        return 1;
    }
    SUBSCRIBE_MSG(s->serviceid, IDUM_CMDRES);
    return 0;
}

static void
_response_error(int id, const char* error) {
    UM_DEF(um, 1024);
    int n = snprintf((char*)(um+1), 1024-UM_HSIZE, "%s", error);
    UM_SENDTOCLI(id, um, UM_HSIZE+n);
}

struct sendud {
    int nsend;
    struct UM_BASE* um;
};

static inline int
_sendto_remote(const struct sc_node* node, void* ud) {
    struct UM_BASE* um = ud->um;
    UM_SEND(node->connid, um, um->msgsz);
    ud->nsend++;
    return 0;
}

static int
_routeto_node(struct server* self, uint16_t nodeid, struct UM_BASE* um) {
    if (nodeid == sc_id()) {
        service_notify_nodemsg(self->ctl_service, -1, um, um->msgsz);
        return 1;
    } else {
        const struct sc_node* node = sc_node_get(nodeid);
        if (node) {
            UM_SENDTONODE(node, um, um->msgsz);
            return 1;
        }
    }
    return 0;
}

static int
_broadcast_type(struct server* self, int tid, struct UM_BASE* um) {
    if (tid == NODE_CENTER) {
        return _routeto_node(self, sc_id(), um); 
    } else {
        struct sendud;
        ud.nsend = 0;
        ud.um = um;
        sc_node_foreach(tid, _sendto_remote, &sendud);
        return sendud.nsend;
    }
}

static int
_broadcast_all(struct server* self, struct UM_BASE* um) {
    int nsend = 0;
    int i;
    for (i=0; i<sc_node_types(); ++i) {
        nsend += _broadcast_type(self, i, um);
    } 
    return nsend;
}

static int
_getnodeid(const char* tidstr, const char* sidstr) {
    if (strcasecmp(tidstr, "all") == 0) {
        return HNODE_ID(HNODE_TID_MAX, HNODE_SID_MAX);
    }
    int tid = sc_node_typeid(tidstr);
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
    struct gate_message* gm = msg;
    struct gate_client* c = sc_gate_getclient(id);
    assert(c);
    UM_CAST(UM_BASE, um, gm->msg);
   
    struct args A;
    args_parsestrl(&A, 3, (char*)(um+1), sz-UM_HSIZE);
    if (A.argc < 1) {
        _response_error(id, "usage: [node sid] command [arg1 arg2 .. ]");
        return;
    }
    int starti = 0;
    int nodeid = HNODE_ID(NODE_CENTER, 0);
    if (A.argc > 2) {
        int id = _getnodeid(A.argv[0], A.argv[1]);
        if (id != -1) {
            nodeid = id;
            starti = 2;
        }
    } 
    int tid = HNODE_TID(nodeid);
    int sid = HNODE_SID(nodeid);
    
    UM_DEFVAR(UM_CMDREQ, req);
    req->cid = id;
    struct memrw rw;
    memrw_init(&rw, req->cmd, req->msgsz - sizeof(*req));
    int i;
    for (i=starti; i<A.argc; ++i) {
        size_t l = strlen(A.argv[i]);
        memrw_write(&rw, A.argv[i], l);
        memrw_write(&rw, " ", 1);
    }
    req->msgsz = sizeof(*req) + RW_CUR(&rw) - 1; // -1 for the last blank

    um = (struct UM_BASE*)req;
    if (tid == HNODE_TID_MAX) {
        _broadcast_all(self, um); 
    } else if (sid == HNODE_SID_MAX) {
        _broadcast_type(self, tid, um);
    } else {
        _routeto_node(self, HNODE_ID(tid, sid), um);
    }
}

static void
_res(struct server* self, int id, struct UM_BASE* um) {
    const struct sc_node* node = sc_node_get(um->nodeid);
    if (node == NULL)
        return;
    UM_CAST(UM_CMDRES, res, um);
    struct gate_client* c = sc_gate_getclient(res->cid);
    if (c == NULL)
        return;
    UM_DEF(notify, UM_MAXSIZE);
    struct memrw rw;
    memrw_init(&rw, notify+1, UM_MAXSIZE-sizeof(*notify));
    char tmp[HNODESTR_MAX]; 
    int n = snprintf(rw.ptr, RW_SPACE(&rw), "%s:\n", 
            sc_strnode(node, tmp));
    memrw_pos(&rw, n);
    size_t sz = res->msgsz - sizeof(*res); 
    memrw_write(&rw, res+1, sz);
    notify->msgid = 0; // just for avoid valgrind
    notify->msgsz = sizeof(*notify) + RW_CUR(&rw);
    UM_SENDTOCLI(c->connid, notify, notify->msgsz);
}

void
cmds_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct server* self = SERVICE_SELF;
    UM_CAST(UM_BASE, um, msg);
    switch (um->msgid) {
    case IDUM_CMDRES:
        _res(self, id, um);
        break;
    }
}

void
cmds_net(struct service* s, struct gate_message* gm) {
    struct gate_client* c = gm->c;
    struct net_message* nm = gm->msg;
    switch (nm->type) {
    case NETE_TIMEOUT:
        _response_error(c->connid, "livetimeout.");
        break;
    }
}
