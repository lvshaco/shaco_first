#include "sc_node.h"
#include "sc_util.h"
#include "sc.h"
#include "sc_service.h"
#include "sc_init.h"
#include "sc_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define SUB_MAX 16

struct _service {
    char name[32];
    int handle;
};

struct _service_array {
    int cap;
    int sz;
    struct _service *p;
};

struct remote {
    int handle;
    struct _service_array sers;
};

static struct remote* R = NULL;

static int
_subscribe(const char *name, int handle) {
    struct _service_array *sers = &R->sers;
    if (name[0] == '\0') {
        sc_error("Subscribe null service");
        return -1;
    }
    int i;
    for (i=0; i<sers->sz; ++i) {
        if (strcmp(sers->p[i].name, name)) {
            sers->p[i].handle = handle;
            return 0x8000 | i;
        }
    }
    if (sers->sz >= SUB_MAX) {
        sc_error("Subscribe too much service");
        return -1;
    }
    if (sers->sz <= sers->cap) {
        sers->cap *= 2;
        if (sers->cap == 0)
            sers->cap = 1;
        sers->p = realloc(sers->p, sizeof(struct _service) * sers->cap);
        memset(sers->p+sers->sz, 0, sers->cap - sers->sz);
    }
    int id = sers->sz++;
    struct _service *s = &sers->p[id];
    sc_strncpy(s->name, name, sizeof(s->name));
    s->handle = handle;
    return 0x8000 | id;
}

int
sc_service_bind(const char *name, int handle) {
    return _subscribe(name, handle);
}

int 
sc_service_subscribe(const char *name) {
    int handle = service_query_id(name);
    if (handle != -1) {
        return handle;
    }
    handle = _subscribe(name, -1);
    if (handle == -1) {
        return -1;
    }
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "SUB %s", name); 
    if (service_main(R->handle, 0, 0, msg, n)) {
        return -1;
    }
    return handle;
}

int 
sc_service_publish(const char *name) {
    int handle = service_query_id(name);
    if (handle == -1) {
        return 1;
    }
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "PUB %s %d", name, handle);
    return service_main(R->handle, 0, 0, msg, n);
}

int 
sc_service_send(int source, int dest, const void *msg, int sz) {
    sc_debug("Command from %0x to %0x", source, dest);
    if (dest & NODE_MASK) {
        service_send(R->handle, 0, source, dest, msg, sz);
    } else {
        service_main(dest, 0, source, msg, sz);
    }
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
        sc_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    sc_service_send(source, dest, msg, n);
    return 0;
}

int 
sc_handler(const char *name, int *handle) {
    *handle = sc_service_subscribe(name);
    return (*handle != -1) ? 0 : 1;
}

static void
sc_node_init() { 
    int handle = service_query_id("node");
    if (handle != -1) {
        if (service_prepare("node")) {
            handle = -1;
            sc_exit("node init fail");
        }
    } else {
        sc_warning("lost node service");
    }
    if (handle != -1) {
        R = malloc(sizeof(*R));
        memset(R, 0, sizeof(*R));
        R->handle = handle;
    }
}

static void
sc_node_fini() {
    if (R == NULL) {
        return;
    }
    free(R->sers.p);
    free(R);
    R = NULL;
}

SC_LIBRARY_INIT_PRIO(sc_node_init, sc_node_fini, 25)
