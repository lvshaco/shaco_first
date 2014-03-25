#include "sh.h"
#include "msg_server.h"
#include "msg_client.h"
#include "cmdctl.h"
#include "memrw.h"
#include "args.h"

struct target_array {
    int cap;
    int sz;
    struct module_info *p;
};

struct route {
    int loadbalance_handle;
    int load_iter;
    struct target_array targets;
};

struct route *
route_create() {
    struct route *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
route_free(struct route *self) {
    if (self == NULL)
        return;
    free(self->targets.p);
    free(self);
}

int
route_init(struct module *s) {
    struct route *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    struct sh_monitor_handle h = { -1, MODULE_ID };
    const char *loadbalance = sh_getstr("route_loadbalance", "");
    if (sh_monitor(loadbalance, &h, &self->loadbalance_handle)) {
        return 1;
    }
    return 0;
}

static inline void
cpy_info(struct module_info *dest, struct module_info *src) {
    *dest = *src; 
    dest->ip[sizeof(dest->ip)-1] = '\0';
}

static struct module_info *
find_module(struct target_array *ts, int handle) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        if (ts->p[i].handle == handle) {
            return &ts->p[i];
        }
    }
    return NULL;
}

static struct module_info *
push_module(struct target_array *ts) {
    if (ts->sz >= ts->cap) {
        ts->cap *= 2;
        if (ts->cap == 0)
            ts->cap = 1;
        ts->p = realloc(ts->p, sizeof(ts->p[0]) * ts->cap);
    }
    return &ts->p[ts->sz++];
}

static int
rm_module(struct target_array *ts, int handle) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        if (ts->p[i].handle == handle) {
            for (; i<ts->sz-1; ++i) {
                ts->p[i] = ts->p[i+1];
            }
            ts->sz--;
            return 0; 
        }
    }
    return 1;
}

static inline void
clr_allmodule(struct target_array *ts) {
    ts->sz = 0;
}

static void
clr_load(struct target_array *ts) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        ts->p[i].load = 0;
    }
}

static struct module_info *
minload_module(struct route *self) {
    int minload = INT_MAX;
    int idx=-1;
    int n = self->targets.sz;
    int i;
    for (i=0; i<n; ++i) {
        idx = (self->load_iter+i) % n;
        if (minload > self->targets.p[idx].load) {
            minload = self->targets.p[idx].load;
            break;
        }
    }
    if (idx != -1) {
        self->load_iter = idx+1;
        return &self->targets.p[idx];
    } else {
        return NULL;
    }
}

static void
gate(struct module *s, int source, int connid, const void *msg, int sz) {
    struct route *self = MODULE_SELF;
   
    UM_CAST(UM_BASE, base, msg);
    if (base->msgid != IDUM_GATEADDRREQ)
        return;

    struct module_info *one;
    one = minload_module(self);
    if (one) {
        UM_DEFWRAP(UM_GATE, ga, UM_GATEADDR, ok);
        ga->connid = connid;
        memcpy(ok->ip, one->ip, sizeof(one->ip));
        ok->port = one->port;
        sh_module_send(MODULE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*ok));
        sh_trace("Route client %d get %s:%u handle %x load %d", 
                connid, one->ip, one->port, one->handle, one->load);
    } else {
        UM_DEFWRAP(UM_GATE, ga, UM_GATEADDRFAIL, fail);
        ga->connid = connid;
        sh_module_send(MODULE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*fail));
        sh_trace("Route client %d get fail", connid);
    }
    {
        UM_DEFWRAP(UM_GATE, ga, UM_LOGOUT, lo);
        ga->connid = connid;
        lo->err = SERR_OKUNFORCE;
        sh_module_send(MODULE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*lo));
    }
}

static void
umsg(struct module *s, int source, const void *msg, int sz) {
    struct route *self = MODULE_SELF;
    UM_CAST(UM_BASE, base, msg);
    switch(base->msgid) {
    case IDUM_GATE: {
        UM_CAST(UM_GATE, g, msg);
        gate(s, source, g->connid, g->wrap, sz-sizeof(*g));
        }
        break;
    case IDUM_SERVICEINIT: {
        UM_CAST(UM_SERVICEINIT, si, msg);
        clr_allmodule(&self->targets);
        struct module_info *one;
        int i;
        for (i=0; i<si->ninfo; ++i) {
            one = push_module(&self->targets);
            assert(one);
            cpy_info(one, &si->info[i]);
            sh_trace("Route handle %x info %s:%u load %d", 
                    one->handle, one->ip, one->port, one->load);
        }
        }
        break;
    case IDUM_SERVICEADD: {
        UM_CAST(UM_SERVICEADD, si, msg);
        struct module_info *one;
        one = find_module(&self->targets, si->info.handle);
        if (one == NULL) {
            one = push_module(&self->targets);
            assert(one);
        }
        cpy_info(one, &si->info);
        sh_trace("Route handle %x info %s:%u load %d", 
                one->handle, one->ip, one->port, one->load);
        }
        break;
    case IDUM_SERVICEDEL: {
        UM_CAST(UM_SERVICEDEL, sd, msg);
        rm_module(&self->targets, sd->handle);
        sh_trace("Route handle %x remove", sd->handle);
        }
        break;
    case IDUM_SERVICELOAD: {
        UM_CAST(UM_SERVICELOAD, sl, msg);
        struct module_info *one = find_module(&self->targets, sl->handle);
        if (one) { 
            one->load = sl->load;
            sh_trace("Route handle %x load %d -> %d", sl->handle, one->load, sl->load);
        } else {
            sh_error("Route handle %x load fail", sl->handle);
        }
        }
        break;
    }
}

static void
monitor(struct module *s, int source, const void *msg, int sz) {
    assert(sz >= 5);
    struct route *self = MODULE_SELF;
    int type = sh_monitor_type(msg);
    switch (type) {
    case MONITOR_EXIT:
        if (sh_monitor_vhandle(msg) == self->loadbalance_handle) {
            // if gateload do not work, clear load of all gate; 
            // then minload_module just work by iterator.
            clr_load(&self->targets);
        }
        break;
    } 
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    struct route *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "list")) {
        struct module_info *one;
        int i;
        for (i=0; i<self->targets.sz; ++i) {
            one = &self->targets.p[i];
            memrw_pos(rw, sh_snprintf(rw->ptr, RW_SPACE(rw), "\r\n  "));
            memrw_pos(rw, sh_snprintf(rw->ptr, RW_SPACE(rw), "[%04x] %s:%u %d", 
                    one->handle, one->ip, one->port, one->load));
        }
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

void
route_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM:
        umsg(s, source, msg, sz);
        break;
    case MT_MONITOR:
        monitor(s, source, msg, sz);
        break;
    case MT_CMD:
        cmdctl(s, source, msg, sz, command);
        break;
    }
}
