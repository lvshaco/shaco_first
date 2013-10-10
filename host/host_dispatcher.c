#include "host_dispatcher.h"
#include "host_service.h"
#include "host_log.h"

static int _DISPATCHER = -1;

int 
host_dispatcher_init() {
    if (service_load("dispatcher")) {
        host_warning("load dispatcher service fail");
    }
    _DISPATCHER = service_query_id("dispatcher");
    return 0;
}

void 
host_dispatcher_fini() {
    _DISPATCHER = -1;
}

int
host_dispatcher_subscribe(int serviceid, int msgid) {
    struct service_message sm;
    sm.sessionid = serviceid; // reuse for serviceid
    sm.source = SERVICE_HOST;
    sm.sz = 0;
    sm.msg = (void*)(intptr_t)msgid; // reuse for msgid
    return service_notify_service(_DISPATCHER, &sm);
}

int
host_dispatcher_publish(struct net_message* nm) {
    return service_notify_net(_DISPATCHER, nm);
}

int 
host_dispatcher_usermsg(void* msg, int sz) {
    return service_notify_usermsg(_DISPATCHER, 0, msg, sz);
}
