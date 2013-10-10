#include "host_service.h"
#include "host.h"
#include "host_log.h"
#include "host_dispatcher.h"
#include "host_gate.h"
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
        host_error("lost cmdctl service");
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
    struct gate_message* gm = msg;
    struct gate_client* c = host_gate_getclient(id);
    assert(c);
    UM_CAST(UM_base, um, gm->msg);
   
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

    UM_DEFVAR(UM_CMDREQ, req);
    req->cid = id;
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
    UM_CAST(UM_CMDRES, res, um);
    struct gate_client* c = host_gate_getclient(res->cid);
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
