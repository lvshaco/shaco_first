#include "host.h"
#include "lur.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_net.h"
#include "host_service.h"
#include "host_dispatcher.h"
#include "host_node.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

struct host {
    const char* file;
    struct lur* cfg;
    bool loop;
};

static struct host* H = NULL;

int
host_getint(const char* key, int def) {
    return lur_getint(H->cfg, key, def);
}

const char*
host_getstr(const char* key, const char* def) {
    return lur_getstr(H->cfg, key, def);
}

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
host_create(const char* file) {
    _install_sighandler();
    host_timer_init();
    service_init(); 
    
    struct lur* L = lur_create(); 
    const char* err = lur_dofile(L, file, "shaco");
    if (!LUR_OK(err)) {
        host_error("%s\n", err);
        lur_free(L);
        return 1;
    }
    H = malloc(sizeof(*H));
    H->loop = true;
    H->file = file;
    H->cfg = L; 
    const char* level = host_getstr("host_loglevel", "");
    if (host_log_init(level)) {
        goto err;
    }
    if (host_dispatcher_init()) {
        goto err;
    }
    if (host_node_init()) {
        goto err;
    } 
    int max = lur_getint(L, "host_connmax", 0);
    if (host_net_init(max)) {
        goto err;
    } 
    const char* service = host_getstr("host_service", "");
    if (service[0] &&
        service_load(service)) {
        goto err;
    }
    if (_checksys()) {
        goto err;
    }
    return 0;
err:
    host_free();
    return 1;
}

void 
host_free() {
    if (H == NULL)
        return;

    host_dispatcher_fini();
    host_node_free();
    host_net_fini();
    host_timer_fini();
    host_log_fini();
    service_fini();
    lur_free(H->cfg);
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
    }
    host_info("Shaco stop");
}

void 
host_stop() {
    H->loop = false;
}
