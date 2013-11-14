#include "host.h"
#include "host_env.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_net.h"
#include "host_service.h"
#include "host_dispatcher.h"
#include "host_node.h"
#include "host_gate.h"
#include "host_reload.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

struct host {
    bool loop;
};

static struct host* H = NULL;

static void 
_sigtermhandler(int sig) {
    host_warning("Received SIGTERM, schedule stop ...");
    host_stop();
} 

static void
_install_sighandler() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = _sigtermhandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

static int
_checksys() {
    struct rlimit l;
    if (getrlimit(RLIMIT_CORE, &l) == -1) {
        host_error("get rlimit core fail: %s", strerror(errno));
        return 1;
    }
    if (l.rlim_cur != RLIM_INFINITY) {
        l.rlim_cur = l.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &l) == -1) {
            host_error("set rlimit fail: %s", strerror(errno));
            return 1;
        }
    }

    int max = host_getint("host_connmax", 0);
    if (getrlimit(RLIMIT_NOFILE, &l) == -1) {
        host_error("get rlimit nofile fail: %s", strerror(errno));
        return 1;
    }
    if (l.rlim_cur < max) {
        l.rlim_cur = max;
        if (l.rlim_max < l.rlim_cur) {
            l.rlim_max = l.rlim_cur;
        }
        if (setrlimit(RLIMIT_NOFILE, &l) == -1) {
            host_error("set rlimit nofile fail: %s", strerror(errno));
            return 1;
        }
    }
    return 0;
}

int 
host_create() {
    H = malloc(sizeof(*H));
    H->loop = true;

    _install_sighandler();
    host_timer_init();
    host_reload_init();
    service_init(); 

    const char* service = host_getstr("host_service", "");
    if (service[0] &&
        service_load(service)) {
        goto errout;
    }
    const char* level = host_getstr("host_loglevel", "");
    if (host_log_init(level)) {
        goto errout;
    }
    int max = host_getint("host_connmax", 0);
    if (host_net_init(max)) {
        goto errout;
    } 
    if (host_dispatcher_init()) {
        goto errout;
    }
    if (host_node_init()) {
        goto errout;
    }
    if (host_gate_init()) {
        goto errout;
    } 
    if (service_prepare(NULL)) {
        goto errout;
    }
    if (_checksys()) {
        goto errout;
    }
    return 0;
errout:
    host_free();
    return 1;
}

void 
host_free() {
    if (H == NULL)
        return;
    
    host_net_fini();
    host_gate_fini();
    host_node_fini();
    host_dispatcher_fini();
    host_log_fini();
    service_fini();
    host_reload_fini();
    host_timer_fini();
    free(H);
    H = NULL;
}

void 
host_start() {
    host_info("Shaco start");
    while (H->loop) {
        int timeout = host_timer_max_timeout();
        host_net_poll(timeout);
        host_timer_dispatch_timeout();
        host_reload_execute();
    }
    host_info("Shaco stop");
}

void 
host_stop() {
    H->loop = false;
}
