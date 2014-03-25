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
    if (sh_monitor(publisher, &h, &self->publisher_vhandle)) {
        return 1;
    }
    const char *subscriber = sh_getstr("loadbalance_subscriber", "");
    if (sh_monitor(subscriber, &h, &self->subscriber_vhandle)) {
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
push_module(struct publisher_vector *ts) {
    if (ts->sz >= ts->cap) {
        ts->cap *= 2;
        if (ts->cap == 0)
            ts->cap = 1;
        ts->p = realloc(ts->p, sizeof(ts->p[0]) * ts->cap);
    }
    return &ts->p[ts->sz++];
}

static int
rm_module(struct publisher_vector *ts, int handle) {
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
    struct module_info *one = find_module(&self->publishers, handle);
    if (one == NULL) {
        one = push_module(&self->publishers);
        assert(one);
        one->handle = handle;
        one->load = 0;
    }
    memcpy(one->ip, msg, 40);
    one->port = sh_from_littleendian16(msg+40);

    UM_DEFVAR(UM_SERVICEADD, sa);
    sa->info = *one;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, sa, sizeof(*sa));
}

static inline void
publisher_exit(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    if (rm_module(&self->publishers, handle)) {
        return;
    }
    UM_DEFFIX(UM_SERVICEDEL, sd);
    sd->handle = handle;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, sd, sizeof(*sd));
}

static inline void
subscriber_start(struct module *s, int handle) {
    struct loadbalance *self = MODULE_SELF;
    UM_DEFVAR(UM_SERVICEINIT, si);
    si->ninfo = self->publishers.sz;
    memcpy(si->info, self->publishers.p, sizeof(si->info[0])* si->ninfo);
    sh_module_send(MODULE_ID, handle, MT_UM, si, UM_SERVICEINIT_size(si));
}

static void
update_load(struct module *s, int handle, int load) {
    struct loadbalance *self = MODULE_SELF;
    struct module_info *one = find_module(&self->publishers, handle);
    if (one == NULL) {
        one = push_module(&self->publishers);
        assert(one);
        one->handle = handle;
    }
    one->load = load;
    if (one->ip[0] == '\0') {
        // update load maybe early monitor start detect
        // so hear just store the load
        return; 
    }
    UM_DEFFIX(UM_SERVICELOAD, sl);
    sl->handle = one->handle;
    sl->load = one->load;
    sh_module_broadcast(MODULE_ID, self->subscriber_vhandle, MT_UM, sl, sizeof(*sl));
}

static void
umsg(struct module *s, int source, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    if (base->msgid == IDUM_UPDATELOAD) {
        UM_CAST(UM_UPDATELOAD, ul, msg);
        update_load(s, source, ul->value);
    }
}

static void
monitor(struct module *s, int source, const void *msg, int sz) {
    struct loadbalance *self = MODULE_SELF;
    assert(sz >= 5);
    int type = sh_monitor_type(msg);
    int vhandle = sh_monitor_vhandle(msg);
    switch (type) {
    case MONITOR_START:
        if (vhandle == self->publisher_vhandle) {
            // todo
            int diff = 5;
            publisher_start(s, source, msg+diff, sz-diff);
        } else if (vhandle == self->subscriber_vhandle) {
            subscriber_start(s, source);
        }
        break;
    case MONITOR_EXIT:
        if (vhandle == self->publisher_vhandle) {
            publisher_exit(s, source);
        }
        break;
    }
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    struct loadbalance *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "list")) {
        struct module_info *one;
        int i;
        for (i=0; i<self->publishers.sz; ++i) {
            one = &self->publishers.p[i];
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
loadbalance_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
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
