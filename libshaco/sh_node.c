#include "sh_node.h"
#include "sh_util.h"
#include "sh.h"
#include "sh_module.h"
#include "sh_init.h"
#include "sh_log.h"
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

struct _module {
    char name[32]; 
    int load_iter;
    int cap;
    int sz;
    struct _handle *phandle;
};

struct _module_vector {
    int cap;
    int sz;
    struct _module *p;
};

struct remote {
    int handle;
    struct _module_vector sers;
};

static struct remote* R = NULL;

// handle
static int
_add_handle(struct _module *s, int handle) {
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
_rm_handle(struct _module *s, int handle) {
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
_first_handle(struct _module *s) {
    if (s->sz > 0) {
        return s->phandle[0].id;
    }
    return -1;
}

static bool
_has_handle(struct _module *s, int handle) {
    int i;
    for (i=0; i<s->sz; ++i) {
        if (s->phandle[i].id == handle)
            return true;
    }
    return false;
}

static inline struct _module *
_get_module(int vhandle) {
    int id = vhandle & SID_MASK;
    if (id >= 0 && id < R->sers.sz) {
        return &R->sers.p[id];
    }
    return NULL;
}

static int
_subscribe(const char *name) {
    struct _module_vector *sers = &R->sers;
    struct _module *s;
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
    sh_strncpy(s->name, name, sizeof(s->name));
    sers->sz++;
    return 0x10000 | n;
}

static int
_register(const char *name, int handle) {
    struct _module_vector *sers = &R->sers;
    struct _module *s;
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
    struct _module_vector *sers = &R->sers;
    struct _module *s;
    int i,j;
    for (i=0; i<sers->sz; ++i) {
        s = &sers->p[i];
        for (j=0; j<s->sz; ++j) {
            if (!_rm_handle(s, handle)) {
                sh_info("Handle(%s:%0x) exit", s->name, handle);
                return 0x10000 | i;
            }
        }
    }
    return -1;
}

int
sh_module_start(const char *name, int handle, const struct sh_node_addr *addr) {
    int vhandle = _register(name, handle);
    if (vhandle != -1) {
        sh_info("Handle(%s:%0x) start", name, handle);
        sh_monitor_trigger_start(vhandle, handle, addr);
        return 0;
    } else
        return 1;
}

int 
sh_module_exit(int handle) {
    int vhandle = _unregister(handle);
    if (vhandle != -1) {
        sh_monitor_trigger_exit(vhandle, handle);
        return 0;
    } else
        return 1;
}

int 
sh_module_subscribe(const char *name, int flag) {
    if (name[0] == '\0') {
        return -1;
    }
    int handle;
    if (flag & SUB_LOCAL) {
        handle = module_query_id(name);
        if (handle != -1) {
            return handle;
        }
    }
    if (flag & SUB_REMOTE) {
        handle = _subscribe(name);
        if (handle != -1) {
            char msg[128];
            int n = snprintf(msg, sizeof(msg), "SUB %s", name); 
            if (module_main(R->handle, 0, 0, MT_TEXT, msg, n)) {
                return -1;
            }
            return handle;
        }
    }
    return -1;
}

int 
sh_module_publish(const char *name, int flag) {
    int handle = module_query_id(name);
    if (handle == -1) {
        return 1;
    }
    if (flag & PUB_SER) {
        char msg[128];
        int n = snprintf(msg, sizeof(msg), "PUB %s:%04x", name, handle);
        module_main(R->handle, 0, 0, MT_TEXT, msg, n);
    }
    if (flag & PUB_MOD) {
        const char *module_name = module_query_module_name(handle);
        if (!(flag & PUB_SER) || strcmp(module_name, name)) {
            char msg[128];
            int n = snprintf(msg, sizeof(msg), "PUB %s:%04x", module_name, handle);
            module_main(R->handle, 0, 0, MT_TEXT, msg, n);
        }
    }
    return 0;
}

static inline void
debug_msg(int source, int dest, int type, const void *msg, int sz) {
    const char *name = "";
    if (!(source >> 8)) {
        name = module_query_module_name(source&0xff);
    }
    switch (type) {
    case MT_TEXT: {
        char tmp[sz+1];
        memcpy(tmp, msg, sz);
        tmp[sz] = '\0';
        sh_debug("[%s - %04x] [T] %s", name, dest, tmp);
        break;
        }
    case MT_UM:
        if (sz >= 2) {
        uint16_t msgid = sh_from_littleendian16((uint8_t *)msg);
        sh_debug("[%s - %04x] [U] %u", name, dest, msgid);
        }
        break;
    } 
}


static inline int
send(int source, int dest, int type, const void *msg, int sz) {
    if (dest & NODE_MASK) {
        debug_msg(source, dest, type, msg, sz);
        return module_send(R->handle, 0, source, dest, type, msg, sz);
    } else {
        return module_main(dest, 0, source, type, msg, sz);
    }
}

int 
sh_module_send(int source, int dest, int type, const void *msg, int sz) {
    if (dest & 0x10000) {
        struct _module *s = _get_module(dest);
        if (s == NULL) {
            sh_error("No subscribe remote module %04x", dest);
            return 1;
        }
        int h = _first_handle(s);
        if (h == -1) {
            sh_error("No connect remote module %s:%04x", s->name, dest);
            return 1;
        }
        dest = h;
    }
    return send(source, dest, type, msg, sz);
}

int 
sh_module_broadcast(int source, int dest, int type, const void *msg, int sz) {
    struct _module *s;
    int i, n=0;
    if (dest & 0x10000) {
        s = _get_module(dest);
        if (s) {
            for (i=0; i<s->sz; ++i) {
                debug_msg(source, dest, type, msg, sz);
                if (!send(source, s->phandle[i].id, type, msg, sz)) {
                    n++;
                }
            }
        }
    }
    return n;
}

int 
sh_module_vsend(int source, int dest, const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sh_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    sh_module_send(source, dest, MT_TEXT, msg, n);
    return 0;
}

int 
sh_module_minload(int vhandle) {
    struct _module *s = _get_module(vhandle);
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
sh_module_nextload(int vhandle) {
    struct _module *s = _get_module(vhandle);
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

bool 
sh_module_has(int vhandle, int handle) {
    struct _module *s = _get_module(vhandle);
    if (s) {
        return _has_handle(s, handle);
    }
    return false;
}

int 
sh_handler(const char *name, int flag, int *handle) {
    *handle = sh_module_subscribe(name, flag);
    if (*handle == -1) {
        sh_error("Subscribe handle %s fail", name);
        return 1;
    }
    return 0;
}

int 
sh_monitor(const char *name, const struct sh_monitor_handle *h, int *handle) {
    *handle = sh_monitor_register(name, h);
    if (*handle == -1) {
        sh_error("Monitor handle %s fail", name);
        return 1;
    }
    return 0;
}
int 
sh_handle_publish(const char *name, int flag) {
    if (sh_module_publish(name, flag)) {
        sh_error("Publish handle %s fail", name);
        return 1;
    }
    return 0;
}

static void
sh_node_init() { 
    int handle = module_query_id("node");
    if (handle != -1) {
        if (module_prepare("node")) {
            handle = -1;
            sh_exit("node init fail");
        }
    } else {
        sh_warning("lost node module");
    }
    if (handle != -1) {
        R = malloc(sizeof(*R));
        memset(R, 0, sizeof(*R));
        R->handle = handle;
    }
}

static void
sh_node_fini() {
    if (R == NULL) {
        return;
    }
    if (R->sers.p) {
        int i;
        for (i=0; i<R->sers.sz; ++i) {
            free(R->sers.p[i].phandle);
        }
        free(R->sers.p);
        R->sers.p = NULL;
        R->sers.sz = 0;
        R->sers.cap = 0;
    }
    free(R);
    R = NULL;
}

SH_LIBRARY_INIT_PRIO(sh_node_init, sh_node_fini, 25)
