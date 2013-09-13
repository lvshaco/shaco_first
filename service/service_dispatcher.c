#include "host_service.h"
#include "host_log.h"
#include "user_message.h"
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
_locate_service(struct _subs* self, int msgid)  {
    if (msgid >= 0 && msgid < UMID_MAX) {
        int serviceid = self->services[msgid];
        if (serviceid != SERVICE_INVALID) {
            host_debug("Receive msg:%d", msgid);
            return serviceid;
        }
    }
    host_debug("Receive invalid msg:%d", msgid);
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

void
dispatcher_net(struct service* s, struct net_message* nm) {
    assert(nm->type == NETE_READ);
    
    struct _subs* self = SERVICE_SELF; 
    int id = nm->connid;
    int serviceid;
    const char* error;
    struct user_message* um = UM_READ(id, &error);
    while (um) {
        serviceid = _locate_service(self, um->msgid);
        if (serviceid != SERVICE_INVALID) {
            service_notify_user_message(serviceid, id, um, um->msgsz);
        }
        host_net_dropread(id);
        um = UM_READ(id, &error);
    }
    if (!NET_OK(error)) {
        // error occur, route to net service
        nm->type = NETE_SOCKERR;
        serviceid = nm->udata; 
        service_notify_net_message(serviceid, nm);
    }
}
