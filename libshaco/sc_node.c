#include "sc_node.h"
#include "sc.h"
#include "sc_service.h"
#include "sc_init.h"
#include "sc_log.h"
#include <stdio.h>
#include <stdarg.h>

static int _SERVICE = -1;

int 
sc_service_subscribe(const char *name) {
    return sc_service_vsend(0, 0, "SUB %s", name);
}

int 
sc_service_publish(const char *name) {
    return sc_service_vsend(0, 0, "PUB %s", name);
}

int 
sc_service_send(int source, int dest, const void *msg, int sz) {
    // todo add net send interface
    //service_send
    return 0;
}

int 
sc_service_vsend(int source, int dest, const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sc_error("Too large message %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    sc_service_send(source, dest, msg, n);
    return 0;
}

static void
sc_node_init() {
    _SERVICE = service_query_id("node");
    if (_SERVICE != -1) {
        if (service_prepare("node")) {
            _SERVICE = -1;
            sc_exit("node init fail");
        }
    } else {
        sc_warning("lost node service");
    }
}

static void
sc_node_fini() {
    _SERVICE = -1;
}

SC_LIBRARY_INIT_PRIO(sc_node_init, sc_node_fini, 25)
