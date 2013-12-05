#include "sc_service.h"
#include "sc_log.h"
#include "message_reader.h"
#include "message_helper.h"
#include "user_message.h"
#include "node_type.h"
#include <assert.h>
#include <string.h>

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
            sc_debug("Receive msg:%d, from %s, to service:%s", 
                    msgid, 
                    sc_node_typename(HNODE_TID(um->nodeid)), 
                    service_query_name(serviceid));
            return serviceid; } }
    sc_debug("Receive invalid msg:%d, from %s", msgid,
            sc_node_typename(HNODE_TID(um->nodeid)));
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
            sc_error("subscribe repeat (service:%s and %s) msgid:%d", 
                    service_query_name(tmp),
                    service_query_name(serviceid), 
                    msgid);
        }
    } else {
        sc_error("subscribe invalid msgid:%d", msgid);
    }
}

void
dispatcher_net(struct service* s, struct net_message* nm) {
    assert(nm->type == NETE_READ);
    struct dispatcher* self = SERVICE_SELF;
    int id = nm->connid; 
    int step = 0; 
    int drop = 1;     
    for (;;) {
        int error = 0;
        struct mread_buffer buf;
        int nread = sc_net_read(id, drop==0, &buf, &error);
        if (nread <= 0) {
            mread_throwerr(nm, error);
            return;
        }
        struct UM_BASE* um;
        while ((um = mread_one(&buf, &error))) {
            int serviceid = _locate_service(self, um);
            if (serviceid != SERVICE_INVALID) {
                service_notify_nodemsg(serviceid, id, um, um->msgsz);
            }
            if (++step > 1000) {
                sc_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (error) {
            sc_net_close_socket(id, true);
            mread_throwerr(nm, error);
            return;
        }
        drop = nread-buf.sz;
        sc_net_dropread(id, drop);       
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
