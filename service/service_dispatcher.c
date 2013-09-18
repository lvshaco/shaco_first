#include "host_service.h"
#include "host_log.h"
#include "user_message.h"
#include "client_type.h"
#include <assert.h>

struct _subs {
    int services[UMID_MAX]; // hold for all subscriber(service id) of msg
};

struct _subs*
dispatcher_create() {
    struct _subs* self = malloc(sizeof(*self));
    int i;
    for (i=0; i<UMID_MAX; ++i) {
        self->services[i] = SERVICE_INVALID;
    }
    return self;
}

static inline int
_locate_service(struct _subs* self, struct UM_base* um)  {
    int msgid = um->msgid;
    int serviceid;
    if (msgid >= 0 && msgid < UMID_MAX) {
        serviceid = self->services[msgid];
        if (serviceid != SERVICE_INVALID) {
            host_debug("Receive msg:%d, from node:%s%04d, to service:%s", 
                    msgid, 
                    host_node_typename(HNODE_TID(um->nodeid)),
                    HNODE_SID(um->nodeid),
                    service_query_name(serviceid));
            return serviceid;
        }
    }
    host_debug("Receive invalid msg:%d, from node:%s%04d", 
            msgid, 
            host_node_typename(HNODE_TID(um->nodeid)), 
            HNODE_SID(um->nodeid));
    return SERVICE_INVALID;
}

void
dispatcher_free(struct _subs* self) {
    if (self == NULL)
        return;
    free(self);
}

void
dispatcher_service(struct service* s, struct service_message* sm) {
    struct _subs* self = SERVICE_SELF;
    int serviceid = sm->sessionid;
    int msgid = (int)(intptr_t)sm->msg;

    if (msgid >= 0 && msgid < UMID_MAX) {
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

static inline struct UM_base*
_read_one(struct net_message* nm, int skip) {
    int id = nm->connid; 
    struct UM_base* base;
    void* data;
    base = host_net_read(id, sizeof(*base), skip);
    if (base == NULL) {
        goto null;
    }
    data = host_net_read(id, base->msgsz + skip - sizeof(*base), 0);
    if (data == NULL) {
        goto null;
    }
    return base;
null:
    if (!NET_OK(host_net_error())) {
        // error occur, route to net service
        nm->type = NETE_SOCKERR;
        service_notify_net(nm->ud, nm);
    }
    return NULL;
}

void
dispatcher_net(struct service* s, struct net_message* nm) {
    assert(nm->type == NETE_READ);
    
    struct _subs* self = SERVICE_SELF; 
    int id = nm->connid;
    int serviceid;
    struct UM_base* um;

    if (nm->ut >= CLI_UNTRUST) {
        // untrust client route to the service of the socket binded
        // and then the service to filter this msg type
        while ((um = _read_one(nm, UM_SKIP)) != NULL) {
            um->msgsz += UM_SKIP;
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
