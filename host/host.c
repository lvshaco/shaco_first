#include "host.h"
#include "lur.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_net.h"
#include "host_service.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct host {
    const char* file;
    struct lur* cfg;
    bool loop;
};

static struct host* H = NULL;

static int
_load_config() {
    struct lur* L = H->cfg;
    const char* err = lur_dofile(L,  H->file);
    if (!LUR_OK(err)) {
        host_error("%s", err);
        return 1;
    }
    if (lur_root(L, "shaco")) {
        host_error("not shaco node");
        return 1;
    }
    return 0;
}

int
host_getint(const char* key, int def) {
    return lur_getint(H->cfg, key, def);
}

const char*
host_getstr(const char* key, const char* def) {
    return lur_getstr(H->cfg, key, def);
}

int 
host_create(const char* file) {
    if (service_init()) {
        return 1;
    }
    if (service_load("log")) {
        printf("load log service fail\n");
        goto err;
    }
    H = malloc(sizeof(*H));
    H->loop = true;
    H->file = file;
    H->cfg = lur_create();

    struct lur* L = H->cfg;
    if (_load_config()) {
        goto err;
    }
    if (host_timer_init()) {
        goto err;
    }
    int max = lur_getint(L, "connection_max", 0);
    if (host_net_init(max)) {
        goto err;
    }
    const char* service = lur_getstr(L, "service", "");
    if (service[0] &&
        service_load(service)) {
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
    host_net_fini();
    host_timer_fini();
    service_fini();
    lur_free(H->cfg);
    free(H);
    H = NULL;
}

void 
host_start() {
    while (H->loop) {
        int timeout = host_timer_max_timeout();
        host_net_poll(timeout);
        host_timer_dispatch_timeout();
    }
}

void 
host_stop() {
    H->loop = false;
}
