#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"

/*
loadbalance_publisher : 负载均衡发布者
loadbalance_subscriber : 负载均衡订阅者
*/

struct publisher_vector {
    int cap;
    int sz;
    struct module_info *p;
};

struct loadbalance {
    int publisher_vhandle;
    int subscriber_vhandle;
    struct publisher_vector publishers;
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
    free(self->publishers.p);
    free(self);
}

int
loadbalance_init(struct module *s) {
    struct loadbalance *self = MODULE_SELF;
    struct sh_monitor_handle h = { MODULE_ID, MODULE_ID };
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    const char *publisher = sh_getstr("loadbalance_publisher", ""); 
    self->publisher_vhandle = sh_monitor_register(publisher, &h);
    if (self->publisher_vhandle == -1) {
        return 1;
    }
    const char *subscriber = sh_getstr("loadbalance_subscriber", "");
    self->subscriber_vhandle = sh_monitor_register(subscriber, &h);
    if(self->subscriber_vhandle == -1) {
        return 1;
    }
    if (self->publisher_vhandle == self->subscriber_vhandle) {
        sh_error("Publisher is equal Subscriber");
        return 1;
    }
    return 0;
}

static struct module_info *
find_module(struct publisher_vector *ts, int handle) {
    int i;
    for (i=0; i<ts->sz; ++i) {
        if (ts->p[i].handle == handle) {
            return &ts->p[i];
        }
    }
    return NULL;
}

static struct module_info *
find_or_insert_module(struct publisher_vector *ts, int handle) {
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
remove_module(struct publisher_vector *ts, int handle) {
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
publisher_start(struct module *s, int handle, const void *msg, int sz) {
    struct loadbalance *self = MODULE_SELF;
    assert(sz >= (40+2));
    struct module_info *one = find_or_insert_module(&self->publishers, handle);
    assert(one);
    memcpy(one->ip, msg, 40);
    one->port = sh_from_littleendian16(msg+40);

    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = 1;
    si->info[0] = *one;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static inline void
publisher_exit(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    if (remove_module(&self->publishers, handle)) {
        return;
    }
    UM_DEFFIX(UM_SERVICEDEL, sd);
    sd->handle = handle;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, sd, sizeof(*sd));
}

static inline void
subscriber_start(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    UM_DEFVAR(UM_SERVICEINFO, si);
    si->ninfo = self->publishers.sz;
    memcpy(si->info, self->publishers.p, sizeof(si->info[0])* si->ninfo);
    sh_module_send(MODULE_ID, handle, MT_UM, si, UM_SERVICEINFO_size(si));
}

static void
update_load(struct module *s, int handle, int load) {
    struct loadbalance *self = MODULE_SELF;
    struct module_info *one = find_or_insert_module(&self->publishers, handle);
    assert(one);
    one->load = load;

    UM_DEFFIX(UM_SERVICELOAD, sl);
    sl->handle = one->handle;
    sl->load = one->load;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, sl, sizeof(*sl));
}

void
loadbalance_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct loadbalance *self = MODULE_SELF;
    switch (type) {
    case MT_MONITOR: {
        assert(sz >= 5);
        int type = ((uint8_t*)(msg))[0];
        int vhandle = sh_from_littleendian32(msg+1);
        if (vhandle == self->publisher_vhandle) {
            switch (type) {
            case MONITOR_START: {
                // todo
                int diff = 5+40+2;
                publisher_start(s, source, msg+diff, sz-diff);
                }
                break;
            case MONITOR_EXIT:
                publisher_exit(s, source);
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
    case MT_CMD:
        cmdctl(s, source, msg, sz, NULL);
        break;
    }
}
