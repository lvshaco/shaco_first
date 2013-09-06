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
    struct lur* L = lur_create(); 
    const char* err = lur_dofile(L, file, "shaco");
    if (!LUR_OK(err)) {
        printf("%s\n", err);
        lur_free(L);
        return 1;
    }
    H = malloc(sizeof(*H));
    H->loop = true;
    H->file = file;
    H->cfg = L; 
    
    if (service_init()) {
        goto err;
    }
    if (service_load("log")) {
        printf("load log service fail\n");
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
