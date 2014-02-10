#include "sc.h"
#include "msg_server.h"
#include "msg_client.h"

struct target_vector {
    int cap;
    int sz;
    struct service_info *p;
};

struct route {
    int load_iter;
    struct target_vector gates;
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
    free(self->gates.p);
    free(self);
}

int
route_init(struct service *s) {
    //struct route *self = SERVICE_SELF;
    if (sh_handle_publish(SERVICE_NAME, PUB_SER)) {
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

static struct service_info *
minload_service(struct route *self) {
    int minload = INT_MAX;
    int idx=-1;
    int n = self->gates.sz;
    int i;
    for (i=0; i<n; ++i) {
        idx = (self->load_iter+i) % n;
        if (minload > self->gates.p[idx].load) {
            minload = self->gates.p[idx].load;
            break;
        }
    }
    if (idx != -1) {
        self->load_iter = idx+1;
        return &self->gates.p[idx];
    } else {
        return NULL;
    }
}

static void
handle_gate(struct service *s, int source, int connid, const void *msg, int sz) {
    struct route *self = SERVICE_SELF;
    
    UM_CASTCK(UM_BASE, base, msg, sz); 
    if (base->msgid != IDUM_GATEADDRREQ) {
        return;
    }
    struct service_info *one = minload_service(self);
    if (one) {
        UM_DEFWRAP(UM_GATE, ga, UM_GATEADDR, ok);
        ga->connid = connid;
        //memcpy(ok->ip, one->ip, sizeof(one->ip));
        memcpy(ok->ip, "116.228.135.50", sizeof(ok->ip));
        ok->port = one->port;
        sh_service_send(SERVICE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*ok));
        sc_trace("Route client %d get %s:%u handle %x load %d", 
                connid, one->ip, one->port, one->handle, one->load);
    } else {
        UM_DEFWRAP(UM_GATE, ga, UM_GATEADDRFAIL, fail);
        ga->connid = connid;
        sh_service_send(SERVICE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*fail));
        sc_trace("Route client %d get fail", connid);
    } 
    {
        UM_DEFWRAP(UM_GATE, ga, UM_LOGOUT, lo);
        ga->connid = connid;
        lo->err = SERR_OK;
        sh_service_send(SERVICE_ID, source, MT_UM, ga, sizeof(*ga) + sizeof(*lo));
    }
}

void
route_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    struct route *self = SERVICE_SELF;
    if (type != MT_UM)
        return;

    UM_CAST(UM_BASE, base, msg);
    switch(base->msgid) {
    case IDUM_GATE: {
        UM_CAST(UM_GATE, g, msg);
        handle_gate(s, source, g->connid, g->wrap, sz-sizeof(*g));
        }
        break;
    case IDUM_SERVICEINFO: {
        UM_CAST(UM_SERVICEINFO, si, msg);
        struct service_info *one;
        int i;
        for (i=0; i<si->ninfo; ++i) {            
            one = find_or_insert_service(&self->gates, si->info[i].handle);
            assert(one);
            *one = si->info[i];
            one->ip[sizeof(one->ip)-1] = '\0';
            sc_trace("Route handle %x info %s:%u load %d", 
                    one->handle, one->ip, one->port, one->load);
        }
        }
        break;
    case IDUM_SERVICEDEL: {
        UM_CAST(UM_SERVICEDEL, sd, msg);
        remove_service(&self->gates, sd->handle);
        sc_trace("Route handle %x remove", sd->handle);
        }
        break;
    case IDUM_SERVICELOAD: {
        UM_CAST(UM_SERVICELOAD, sl, msg);
        struct service_info *one = find_or_insert_service(&self->gates, sl->handle);
        assert(one);
        one->load = sl->load;
        sc_trace("Route handle %x load %d update_load %d", sl->handle, one->load, sl->load);
        }
        break;
    }
}
