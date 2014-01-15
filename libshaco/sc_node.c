#include "sc_node.h"
#include "sh_util.h"
#include "sc.h"
#include "sc_service.h"
#include "sc_init.h"
#include "sc_log.h"
#include "sh_monitor.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define SID_MASK 0xff
#define SUB_MAX  0xff
#define NODE_MASK 0xff00

struct _handle {
    int id;
    int load;
};

struct _service {
    char name[32]; 
    int load_iter;
    int cap;
    int sz;
    struct _handle *phandle;
};

struct _service_vector {
    int cap;
    int sz;
    struct _service *p;
};

struct remote {
    int handle;
    struct _service_vector sers;
};

static struct remote* R = NULL;

// handle
static int
_add_handle(struct _service *s, int handle) {
    if (handle == -1) {
        return 1;
    }
    int n = s->sz;
    if (n >= s->cap) {
        s->cap *= 2;
        if (s->cap == 0)
            s->cap = 1;
        s->phandle = realloc(s->phandle, sizeof(s->phandle[0]) * s->cap);
    }
    s->phandle[n].id = handle;
    s->phandle[n].load = 0;
    s->sz++;
    return 0;
}

static int
_rm_handle(struct _service *s, int handle) {
    if (handle == -1) {
        return 1;
    }
    int i;
    for (i=0; i<s->sz; ++i) {
        if (s->phandle[i].id == handle) {
            for (; i<s->sz-1; ++i) {
                s->phandle[i] = s->phandle[i+1];
            }
            s->sz--;
            return 0;
        }
    }
    return 1;
}

static inline int
_first_handle(struct _service *s) {
    if (s->sz > 0) {
        return s->phandle[0].id;
    }
    return -1;
}

static inline struct _service *
_get_service(int vhandle) {
    int id = vhandle & SID_MASK;
    if (id >= 0 && id < R->sers.sz) {
        return &R->sers.p[id];
    }
    return NULL;
}

static int
_subscribe(const char *name) {
    struct _service_vector *sers = &R->sers;
    struct _service *s;
    int i;
    int n = sers->sz;
    for (i=0; i<n; ++i) {
        s = &sers->p[i];
        if (!strcmp(s->name, name)) {
            return 0x10000 | i;
        }
    }
    if (n >= sers->cap) {
        sers->cap *= 2;
        if (sers->cap == 0)
            sers->cap = 1;
        sers->p = realloc(sers->p, sizeof(sers->p[0]) * sers->cap);
    } 
    s = &sers->p[n];
    memset(s, 0, sizeof(*s));
    sc_strncpy(s->name, name, sizeof(s->name));
    sers->sz++;
    return 0x10000 | n;
}

static int
_register(const char *name, int handle) {
    struct _service_vector *sers = &R->sers;
    struct _service *s;
    int i,j;
    for (i=0; i<sers->sz; ++i) {
        s = &sers->p[i];
        for (j=0; j<s->sz; ++j) {
            if (s->phandle[j].id == handle) {
                return 1;
            }
        }
    }
    for (i=0; i<sers->sz; ++i) {
        s = &sers->p[i];
        if (!strcmp(s->name, name)) {
            if (!_add_handle(s, handle)) {
                return 0x10000 | i;
            } else
                return -1;
        }
    }
    return -1;
}

static int
_unregister(int handle) {
    struct _service_vector *sers = &R->sers;
    struct _service *s;
    int i,j;
    for (i=0; i<sers->sz; ++i) {
        s = &sers->p[i];
        for (j=0; j<s->sz; ++j) {
            if (!_rm_handle(s, handle)) {
                return 0x10000 | i;
            }
        }
    }
    return -1;
}

int
sc_service_start(const char *name, int handle, const struct sh_node_addr *addr) {
    int vhandle = _register(name, handle);
    if (vhandle != -1) {
        sh_monitor_trigger_start(vhandle, handle, addr);
        return 0;
    } else
        return 1;
}

int 
sc_service_exit(int handle) {
    int vhandle = _unregister(handle);
    if (vhandle != -1) {
        sh_monitor_trigger_exit(vhandle, handle);
        return 0;
    } else
        return 1;
}

int 
sc_service_subscribe(const char *name) {
    int handle = service_query_id(name);
    if (handle != -1) {
        return handle;
    }
    handle = _subscribe(name);
    if (handle == -1) {
        return -1;
    }
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "SUB %s", name); 
    if (service_main(R->handle, 0, 0, MT_TEXT, msg, n)) {
        return -1;
    }
    return handle;
}

int 
sc_service_publish(const char *name, int flag) {
    int handle = service_query_id(name);
    if (handle == -1) {
        return 1;
    }
    if (flag & 1) {
        char msg[128];
        int n = snprintf(msg, sizeof(msg), "PUB %s %d", name, handle);
        service_main(R->handle, 0, 0, MT_TEXT, msg, n);
    }
    if (flag & 2) {
        const char *module_name = service_query_module_name(handle);
        if (!(flag&1) || strcmp(module_name, name)) {
            char msg[128];
            int n = snprintf(msg, sizeof(msg), "PUB %s %d", module_name, handle);
            service_main(R->handle, 0, 0, MT_TEXT, msg, n);
        }
    }
    return 0;
}

int 
sh_service_send(int source, int dest, int type, const void *msg, int sz) {
    sc_debug("Command from %0x to %0x", source, dest);
    if (dest & 0x10000) {
        struct _service *s = _get_service(dest);
        if (s == NULL) {
            sc_error("No subscribe remote service %d", dest);
            return 1;
        }
        int h = _first_handle(s);
        if (h == -1) {
            sc_error("No connect remote service %s:%d", s->name, dest);
            return 1;
        }
        return service_send(R->handle, 0, source, h, type, msg, sz);
    }
    if (dest & NODE_MASK) {
        return service_send(R->handle, 0, source, dest, type, msg, sz);
    } else {
        return service_main(dest, 0, source, type, msg, sz);
    }
    return 0;
}

int 
sc_service_broadcast(int source, int dest, int type, const void *msg, int sz) {
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
    sh_service_send(source, dest, MT_TEXT, msg, n);
    return 0;
}

int 
sc_service_minload(int vhandle) {
    struct _service *s = _get_service(vhandle);
    if (s == NULL) {
        return -1;
    }
    if (s->sz <= 0) {
        return -1;
    }
    if (s->load_iter >= s->sz) {
        s->load_iter = 0;
    }
    return s->phandle[s->load_iter++].id;
}

int 
sc_service_nextload(int vhandle) {
    struct _service *s = _get_service(vhandle);
    if (s == NULL) {
        return -1;
    }
    int minload = INT_MAX;
    int idx=-1;
    int n = s->sz;
    int i;
    for (i=0; i<n; ++i) {
        idx = (s->load_iter+i) % n;
        if (minload > s->phandle[idx].load) {
            minload = s->phandle[idx].load;
            break;
        }
    }
    if (idx != -1) {
        s->load_iter = idx+1;
        return s->phandle[idx].id;
    } else {
        return -1;
    }
}

int 
sh_handler(const char *name, int *handle) {
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
