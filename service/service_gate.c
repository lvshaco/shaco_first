#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host_dispatcher.h"
#include "host_timer.h"
#include "host.h"
#include "host_gate.h"
#include "message_reader.h"
#include "user_message.h"
#include "client_type.h"
#include "node_type.h"
#include "message.h"
#include "cli_message.h"
#include <stdlib.h>
#include <assert.h>

/*
 * control the client connect, login, heartbeat, and logout, 
 * the handler service only focus on logined client
 */

struct gate {
    int handler;
    int livetime;
    bool need_verify;
    bool need_load;
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
    int wbuffermax = host_getint("gate_wbuffermax", 0);
    if (addr[0] == '\0')
        return 1;
    if (host_net_listen(addr, port, wbuffermax, s->serviceid, CLI_GAME)) {
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

    self->need_verify = host_getint("gate_need_verify", 1);
    self->need_load = host_getint("gate_need_load", 0);
    int live = host_getint("gate_clientlive", 3);
    self->livetime = live * 1000;
    host_timer_register(s->serviceid, 1000);
    return 0;
}

static inline void
_handlemsg(struct gate* self, struct gate_client* c, struct UM_BASE* um) {
    if (c->status == GATE_CLIENT_LOGINED) {
        c->active_time = host_timer_now();
    }
    if (um->msgid != IDUM_HEARTBEAT) {
        host_debug("Receive msg:%u",  um->msgid);
        struct gate_message gm;
        gm.c = c;
        gm.msg = um;
        service_notify_usermsg(self->handler, c->connid, &gm, um->msgsz);
    }
}

static void
_read(struct gate* self, struct gate_client* c, struct net_message* nm) {
    int n = 0;
    struct UM_BASE* um;
    while ((um = _message_read_one(nm, UM_SKIP)) != NULL) {
        if (um->msgsz > UM_CLIMAX) {
            host_net_close_socket(nm->connid, true);
            nm->type = NETE_SOCKERR;
            nm->error = NET_ERR_MSG;
            service_notify_net(nm->ud, nm);
            break;
        }
        _handlemsg(self, c, um);
        host_net_dropread(nm->connid, UM_SKIP);
        if (++n > 10)
            break;
    }
}

static inline void
_updateload() {
    const struct host_node* node = host_node_get(HNODE_ID(NODE_LOAD, 0));
    if (node) {
        UM_DEFFIX(UM_UPDATELOAD, load);
        load->value = host_gate_usedclient();
        host_debug("update load %d", load->value);
        UM_SENDTONODE(node, load, load->msgsz);
    }
}

void
gate_service(struct service* s, struct service_message* sm) {
    struct gate* self = SERVICE_SELF;
    if (self->need_load) {
        _updateload();
    }
}

void
gate_net(struct service* s, struct net_message* nm) {
    struct gate* self = SERVICE_SELF;
    struct gate_client* c;
    int id = nm->connid;
    switch (nm->type) {
    case NETE_READ: {
        c = host_gate_getclient(id); 
        assert(c);
        _read(self, c, nm);
        }
        break;
    case NETE_ACCEPT:
        // do not forward to handler
        c = host_gate_acceptclient(id);
        host_debug("accept %d", id);
        if (!self->need_verify && c) {
            host_gate_loginclient(c);
        }
        break;
    case NETE_SOCKERR: {
        c = host_gate_getclient(id);
        assert(c);
        if (c->status == GATE_CLIENT_LOGINED) {
            struct gate_message gm;
            gm.c = c;
            gm.msg = nm;
            service_notify_net(self->handler, (void*)&gm);
        }
        host_gate_disconnclient(c, true);
        }
        break;
    case NETE_WRIDONECLOSE: {
        // donot forward to handler
        c = host_gate_getclient(id);
        assert(c);
        host_gate_disconnclient(c, true); 
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
        switch (c->status) {
        case GATE_CLIENT_CONNECTED:
            if (now - c->active_time > 5*1000) {
                host_debug("login timeout");
                host_gate_disconnclient(c, true);
            }
            break;
        case GATE_CLIENT_LOGINED:
            if (self->livetime > 0 &&
                self->livetime < now - c->active_time) {
                host_debug("heartbeat timeout");
                struct gate_message gm;
                struct net_message nm;
                nm.type = NETE_TIMEOUT;
                gm.c = c;
                gm.msg = &nm;
                service_notify_net(self->handler, (void*)&gm);
                host_gate_disconnclient(c, true);
            }
            break;
        case GATE_CLIENT_LOGOUTED:
            if (now - c->active_time > 2*1000) {
                host_debug("logout timeout");
                host_gate_disconnclient(c, true);
            }
            break;
        default:
            break;
        }
    }
}
