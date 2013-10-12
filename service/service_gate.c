#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host_dispatcher.h"
#include "host_timer.h"
#include "host.h"
#include "host_gate.h"
#include "client_type.h"
#include "message.h"
#include "cli_message.h"
#include <stdlib.h>
#include <assert.h>

struct gate {
    int handler;
    int livetime;
};

struct gate*
gate_create() {
    struct gate* self = malloc(sizeof(*self));
    self->handler = SERVICE_INVALID;
    return self;
}

void
gate_free(struct gate* self) {
    free(self);
}

static int
_listen(struct service* s) {
    const char* addr = host_getstr("gate_ip", "");
    int port = host_getint("gate_port", 0);
    if (addr[0] != '\0' &&
        host_net_listen(addr, port, s->serviceid, CLI_GAME)) {
        host_error("listen gate fail");
        return 1;
    }
    return 0;
}
int
gate_init(struct service* s) {
    struct gate* self = SERVICE_SELF;
    const char* hname = host_getstr("gate_handler", "");
    self->handler = service_query_id(hname);
    if (self->handler == SERVICE_INVALID) {
        host_error("lost gate handler service");
        return 1;
    }
    if (_listen(s))
        return 1;
    int cmax = host_getint("gate_clientmax", 0);
    int hmax = host_getint("host_connmax", cmax);
    if (host_gate_prepare(cmax, hmax)) {
        return 1;
    }
    host_info("gate_clientmax = %d", cmax);

    int live = host_getint("gate_clientlive", 3);
    self->livetime = live * 1000;
    host_timer_register(s->serviceid, self->livetime);
    return 0;
}

void
gate_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate* self = SERVICE_SELF;
    struct gate_client* c = host_gate_getclient(id);
    assert(c);
    c->active_time = host_timer_now();

    UM_CAST(UM_BASE, um, msg);
    if (um->msgid != IDUM_HEARTBEAT) {
        host_debug("Receive msg:%u",  um->msgid);
        struct gate_message gm;
        gm.c = c;
        gm.msg = msg;
        service_notify_usermsg(self->handler, id, &gm, sz);
    }
}

void
gate_net(struct service* s, struct net_message* nm) {
    struct gate* self = SERVICE_SELF;
    struct gate_client* c;
    switch (nm->type) {
    case NETE_ACCEPT: {
        uint64_t now = host_timer_now();
        c = host_gate_acceptclient(nm->connid, now);
        }
        break;
    case NETE_SOCKERR: {
        c = host_gate_getclient(nm->connid);
        assert(c);
        struct gate_message gm;
        gm.c = c;
        gm.msg = nm;
        service_notify_net(self->handler, (void*)&gm);
        host_gate_disconnclient(c, false);
        }
        break;
    }
}

void
gate_time(struct service* s) {
    struct gate* self = SERVICE_SELF; 
    uint64_t now = host_timer_now();
    
    struct gate_client* p = host_gate_firstclient();
    struct gate_client* c;
    int max = host_gate_maxclient();
    int i;
    for (i=0; i<max; ++i) {
        c = &p[i];
        if (!c->connected) {
            continue;
        }
        if (now > c->active_time &&
            now - c->active_time > self->livetime) {
            struct gate_message gm;
            struct net_message nm;
            nm.type = NETE_TIMEOUT;
            gm.c = c;
            gm.msg = &nm;
            service_notify_net(self->handler, (void*)&gm);
            host_gate_disconnclient(c, true);
        }
    }
}
