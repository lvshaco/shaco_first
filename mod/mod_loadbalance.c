#include "sh.h"
#include "msg_server.h"

/*
loadbalance_target : 负载均衡目标
loadbalance_subshriber : 负载均衡目标的订阅者
*/

struct target_vector {
    int cap;
    int sz;
    struct module_info *p;
};

struct loadbalance {
    int target_vhandle;
    int subshriber_vhandle;
    struct target_vector targets;
};

struct loadbalance *
loadbalance_create() {
    struct loadbalance *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
loadbalance_free(struct loadbalance *self) {
    if (self == NULL)
        return;
    free(self->targets.p);
    free(self);
}

int
loadbalance_init(struct module *s) {
    struct loadbalance *self = MODULE_SELF;
    struct sh_monitor_handle h = { MODULE_ID, MODULE_ID };
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    const char *target = sh_getstr("loadbalance_target", ""); 
    self->target_vhandle = sh_monitor_register(target, &h);
    if (self->target_vhandle == -1) {
        return 1;
    }
    const char *subshriber = sh_getstr("loadbalance_subshriber", "");
    self->subshriber_vhandle = sh_monitor_register(subshriber, &h);
    if(self->subshriber_vhandle == -1) {
        return 1;
    }
    if (self->target_vhandle == self->subshriber_vhandle) {
        sh_error("Target is equal Subshriber");
        return 1;
    }
    return 0;
}

static struct module_info *
find_module(struct target_vector *ts, int handle) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        if (ts->p[i].handle == handle) {
            return &ts->p[i];
        }
    }
    return NULL;
}

static struct module_info *
find_or_insert_module(struct target_vector *ts, int handle) {
    struct module_info *one = find_module(ts, handle);
    if (one) {
        return one;
    }
    if (ts->sz >= ts->cap) {
        ts->cap *= 2;
        if (ts->cap == 0)
            ts->cap = 1;
        ts->p = realloc(ts->p, sizeof(ts->p[0]) * ts->cap);
    }
    one = &ts->p[ts->sz++];
    memset(one, 0, sizeof(*one));
    one->handle = handle;
    return one;
}

static int
remove_module(struct target_vector *ts, int handle) {
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
target_start(struct module *s, int handle, const void *msg, int sz) {
    struct loadbalance *self = MODULE_SELF;
    assert(sz >= (40+2));
    struct module_info *one = find_or_insert_module(&self->targets, handle);
    assert(one);
    memcpy(one->ip, msg, 40);
    one->port = sh_from_littleendian16(msg+40);

    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = 1;
    si->info[0] = *one;
    sh_module_broadcast(MODULE_ID, self->subshriber_vhandle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static inline void
target_exit(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    if (remove_module(&self->targets, handle)) {
        return;
    }
    UM_DEFFIX(UM_SERVICEDEL, sd);
    sd->handle = handle;
    sh_module_broadcast(MODULE_ID, self->subshriber_vhandle, MT_UM, sd, sizeof(*sd));
}

static inline void
subshriber_start(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = self->targets.sz;
    memcpy(si->info, self->targets.p, sizeof(si->info[0])* si->ninfo);
    sh_module_send(MODULE_ID, handle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static void
update_load(struct module *s, int handle, int load) {
    struct loadbalance *self = MODULE_SELF;
    struct module_info *one = find_or_insert_module(&self->targets, handle);
    assert(one);
    one->load = load;

    UM_DEFFIX(UM_SERVICELOAD, sl);
    sl->handle = one->handle;
    sl->load = one->load;
    sh_module_broadcast(MODULE_ID, self->subshriber_vhandle, MT_UM, sl, sizeof(*sl));
}

void
loadbalance_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct loadbalance *self = MODULE_SELF;
    switch (type) {
    case MT_MONITOR: {
        assert(sz >= 5);
        int type = ((uint8_t*)(msg))[0];
        int vhandle = sh_from_littleendian32(msg+1);
        if (vhandle == self->target_vhandle) {
            switch (type) {
            case MONITOR_START: {
                // todo
                int diff = 5+40+2;
                target_start(s, source, msg+diff, sz-diff);
                }
                break;
            case MONITOR_EXIT:
                target_exit(s, source);
                break;
            }
        }
        if (vhandle == self->subshriber_vhandle) {
            if (type == MONITOR_START) {
                subshriber_start(s, source);
            }
        }
        break;
        }
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        if (base->msgid == IDUM_UPDATELOAD) {
            UM_CAST(UM_UPDATELOAD, ul, msg);
            update_load(s, source, ul->value);
        }
        break;
        }
    }
}