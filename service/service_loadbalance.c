#include "sc_service.h"
#include "sh_monitor.h"
#include "sc_node.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sh_util.h"
#include "user_message.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*
loadbalance_target : 负载均衡目标
loadbalance_subscriber : 负载均衡目标的订阅者
*/

struct target_vector {
    int cap;
    int sz;
    struct service_info *p;
};

struct loadbalance {
    int target_vhandle;
    int subscriber_vhandle;
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
loadbalance_init(struct service *s) {
    struct loadbalance *self = SERVICE_SELF;
    struct sh_monitor_handle h = { SERVICE_ID, SERVICE_ID };

    const char *target = sc_getstr("loadbalance_target", ""); 
    self->target_vhandle = sh_monitor_register(target, &h);
    if (self->target_vhandle == -1) {
        return 1;
    }
    const char *subscriber = sc_getstr("loadbalance_subscriber", "");
    self->subscriber_vhandle = sh_monitor_register(subscriber, &h);
    if(self->subscriber_vhandle == -1) {
        return 1;
    }
    if (self->target_vhandle == self->subscriber_vhandle) {
        sc_error("Target is equal Subscriber");
        return 1;
    }
    return 0;
}

static struct service_info *
find_service(struct target_vector *ts, int handle) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        if (ts->p[i].handle == handle) {
            return &ts->p[i];
        }
    }
    return NULL;
}

static struct service_info *
find_or_insert_service(struct target_vector *ts, int handle) {
    struct service_info *one = find_service(ts, handle);
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
remove_service(struct target_vector *ts, int handle) {
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
target_start(struct service *s, int handle, const void *msg, int sz) {
    struct loadbalance *self = SERVICE_SELF;
    assert(sz >= (40+2));
    struct service_info *one = find_or_insert_service(&self->targets, handle);
    assert(one);
    memcpy(one->ip, msg, 40);
    one->port = sh_from_bigendian16(msg+40);

    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = 1;
    si->info[0] = *one;
    sh_service_broadcast(SERVICE_ID, self->subscriber_vhandle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static inline void
target_exit(struct service *s, int handle) {
    struct loadbalance *self = SERVICE_SELF;
    if (remove_service(&self->targets, handle)) {
        return;
    }
    UM_DEFFIX(UM_SERVICEDEL, sd);
    sd->handle = handle;
    sh_service_broadcast(SERVICE_ID, self->subscriber_vhandle, MT_UM, sd, sizeof(*sd));
}

static inline void
subscriber_start(struct service *s, int handle) {
    struct loadbalance *self = SERVICE_SELF;
    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = self->targets.sz;
    memcpy(si->info, self->targets.p, sizeof(si->info[0])* si->ninfo);
    sh_service_send(SERVICE_ID, handle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static void
update_load(struct service *s, int handle, int load) {
    struct loadbalance *self = SERVICE_SELF;
    struct service_info *one = find_or_insert_service(&self->targets, handle);
    assert(one);
    one->load = load;

    UM_DEFFIX(UM_SERVICELOAD, sl);
    sl->handle = one->handle;
    sl->load = one->load;
    sh_service_broadcast(SERVICE_ID, self->subscriber_vhandle, MT_UM, sl, sizeof(*sl));
}

void
loadbalance_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    struct loadbalance *self = SERVICE_SELF;
    switch (type) {
    case MT_MONITOR: {
        assert(sz >= 5);
        int type = ((uint8_t*)(msg))[0];
        int vhandle = sh_from_bigendian32(msg+1);
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
        if (vhandle == self->subscriber_vhandle) {
            if (type == MONITOR_START) {
                subscriber_start(s, source);
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
