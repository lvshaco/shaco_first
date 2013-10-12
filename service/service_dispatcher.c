#include "host_service.h"
#include "host_log.h"
#include "user_message.h"
#include "client_type.h"
#include "node_type.h"
#include <assert.h>

struct dispatcher {
    int services[IDUM_MAX]; // hold for all subscriber(service id) of msg
};

struct dispatcher*
dispatcher_create() {
    struct dispatcher* self = malloc(sizeof(*self));
    int i;
    for (i=0; i<IDUM_MAX; ++i) {
        self->services[i] = SERVICE_INVALID;
    }
    return self;
}

static inline int
_locate_service(struct dispatcher* self, struct UM_BASE* um)  {
    int msgid = um->msgid;
    int serviceid;
    if (msgid >= 0 && msgid < IDUM_MAX) {
        serviceid = self->services[msgid];
        if (serviceid != SERVICE_INVALID) {
            host_debug("Receive msg:%d, from %s, to service:%s", 
                    msgid, 
                    host_node_typename(HNODE_TID(um->nodeid)), 
                    service_query_name(serviceid));
            return serviceid;
        }
    }
    host_debug("Receive invalid msg:%d, from %s", msgid,
            host_node_typename(HNODE_TID(um->nodeid)));
    return SERVICE_INVALID;
}

void
dispatcher_free(struct dispatcher* self) {
    if (self == NULL)
        return;
    free(self);
}

void
dispatcher_service(struct service* s, struct service_message* sm) {
    struct dispatcher* self = SERVICE_SELF;
    int serviceid = sm->sessionid;
    int msgid = (int)(intptr_t)sm->msg;

    if (msgid >= 0 && msgid < IDUM_MAX) {
        int tmp = self->services[msgid];
        if (tmp == SERVICE_INVALID) {
            self->services[msgid] = serviceid; 
        } else {
            host_error("subscribe repeat (service:%s and %s) msgid:%d", 
                    service_query_name(tmp),
                    service_query_name(serviceid), 
                    msgid);
        }
    } else {
        host_error("subscribe invalid msgid:%d", msgid);
    }
}

static inline struct UM_BASE*
_read_one(struct net_message* nm, int skip) {
    int id = nm->connid; 
    struct UM_BASE* base;
    void* data;
    base = host_net_read(id, sizeof(*base), skip);
    if (base == NULL) {
        goto null;
    }
    int sz = base->msgsz + skip - sizeof(*base);
    if (sz != 0) {
        data = host_net_read(id, sz, 0);
        if (data == NULL) {
            goto null;
        }
    }
    return base;
null:
    if (host_net_socket_isclosed(id)) {
        // error occur, route to net service
        nm->type = NETE_SOCKERR;
        service_notify_net(nm->ud, nm);
    }
    return NULL;
}

void
dispatcher_net(struct service* s, struct net_message* nm) {
    assert(nm->type == NETE_READ);
    
    struct dispatcher* self = SERVICE_SELF; 
    int id = nm->connid;
    int serviceid;
    struct UM_BASE* um;

    if (nm->ut >= CLI_UNTRUST) {
        // untrust client route to the service of the socket binded
        // and then the service to filter this msg type
        while ((um = _read_one(nm, UM_SKIP)) != NULL) {
            um->msgsz += UM_SKIP;
            if (um->msgsz > UM_CLIMAX) {
                host_net_close_socket(id);
                nm->type = NETE_SOCKERR;
                service_notify_net(nm->ud, nm);
            }
            service_notify_usermsg(nm->ud, id, um, um->msgsz);
            host_net_dropread(id, UM_SKIP);
        }
    } else {
        // trust client route to the subscribe service directly
        while ((um = _read_one(nm, 0)) != NULL) { 
            serviceid = _locate_service(self, um);
            if (serviceid != SERVICE_INVALID) {
                service_notify_nodemsg(serviceid, id, um, um->msgsz);
            }
            host_net_dropread(id, 0);
        }
    }
}

void
dispatcher_usermsg(struct service* s, int id, void* msg, int sz) {
    struct dispatcher* self = SERVICE_SELF; 
    struct node_message* nm = msg;
    int serviceid = _locate_service(self, nm->um);
    if (serviceid != SERVICE_INVALID) {
        service_notify_usermsg(serviceid, id, msg, sz);
    }
}
