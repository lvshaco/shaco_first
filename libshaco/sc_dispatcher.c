#include "sc_dispatcher.h"
#include "sc.h"
#include "sc_init.h"
#include "sc_service.h"
#include "sc_log.h"
#include "sc_init.h"

static int _DISPATCHER = SERVICE_INVALID;

int
sc_dispatcher_subscribe(int serviceid, int msgid) {
    struct service_message sm;
    sm.sessionid = serviceid; // reuse for serviceid
    sm.source = SERVICE_HOST;
    sm.sz = 0;
    sm.msg = (void*)(intptr_t)msgid; // reuse for msgid
    return service_notify_service(_DISPATCHER, &sm);
}

int
sc_dispatcher_publish(struct net_message* nm) {
    return service_notify_net(_DISPATCHER, nm);
}

int 
sc_dispatcher_usermsg(void* msg, int sz) {
    return service_notify_usermsg(_DISPATCHER, 0, msg, sz);
}

static void
sc_dispatcher_init() {
    _DISPATCHER = service_query_id("dispatcher");
    if (_DISPATCHER != SERVICE_INVALID) {
        if (service_prepare("dispatcher")) {
            sc_exit("dispatcher init fail");
        }
    } else {
        sc_warning("lost dispatcher service");
    }
}

static void 
sc_dispatcher_fini() {
    _DISPATCHER = SERVICE_INVALID;
}

SC_LIBRARY_INIT_PRIO(sc_dispatcher_init, sc_dispatcher_fini, 25);
