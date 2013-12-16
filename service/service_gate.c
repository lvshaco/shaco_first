#include "sc_service.h"
#include "sc_env.h"
#include "sc_net.h"
#include "sc_log.h"
#include "sc_dispatcher.h"
#include "sc_timer.h"
#include "sc.h"
#include "sc_gate.h"
#include "message_reader.h"
#include "message_helper.h"
#include "user_message.h"
#include "client_type.h"
#include "node_type.h"
#include "message.h"
#include "cli_message.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

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
    const char* addr = sc_getstr("gate_ip", "");
    int port = sc_getint("gate_port", 0);
    int wbuffermax = sc_getint("gate_wbuffermax", 0);
    if (addr[0] == '\0')
        return 1;
    if (sc_net_listen(addr, port, wbuffermax, s->serviceid, CLI_GAME)) {
        sc_error("listen gate fail");
        return 1;
    }
    return 0;
}
int
gate_init(struct service* s) {
    struct gate* self = SERVICE_SELF;
    const char* hname = sc_getstr("gate_handler", "");
    if (sc_handler(hname, &self->handler))
        return 1;
    if (_listen(s))
        return 1;
    int cmax = sc_getint("gate_clientmax", 0);
    int hmax = sc_getint("sc_connmax", cmax);
    if (sc_gate_prepare(cmax, hmax)) {
        return 1;
    }
    sc_info("gate_clientmax = %d", cmax);

    self->need_verify = sc_getint("gate_verify", 1);
    self->need_load = sc_getint("gate_load", 0);
    int live = sc_getint("gate_clientlive", 3);
    self->livetime = live * 1000;
    sc_timer_register(s->serviceid, 1000);
    return 0;
}

static inline void
_handlemsg(struct gate* self, struct gate_client* c, struct UM_BASE* um) {
    if (c->status == GATE_CLIENT_LOGINED) {
        c->active_time = sc_timer_now();
    }
    if (um->msgid != IDUM_HEARTBEAT) {
        sc_debug("Receive msg:%u",  um->msgid);
        struct gate_message gm;
        gm.c = c;
        gm.msg = um;
        service_notify_usermsg(self->handler, c->connid, &gm, um->msgsz);
    }
}

static void
_read(struct gate* self, struct gate_client* c, struct net_message* nm) {
    int id = nm->connid;
    int step = 0;
    int drop = 1;
    for (;;) {
        int error = 0;
        struct mread_buffer buf;
        int nread = sc_net_read(id, drop==0, &buf, &error);
        if (nread <= 0) {
            mread_throwerr(nm, error);
            return;
        }
        struct UM_CLI_BASE* one;
        while ((one = mread_cli_one(&buf, &error))) {
            // copy to stack buffer, besafer
            UM_DEF(msg, UM_CLI_MAXSZ); 
            msg->nodeid = 0;
            memcpy(&msg->cli_base, one, UM_CLI_SZ(one));
            _handlemsg(self, c, msg);
            if (++step > 10) {
                sc_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (error) {
            sc_net_close_socket(id, true);
            mread_throwerr(nm, error);
            return;
        }
        drop = nread - buf.sz;
        sc_net_dropread(id, drop);       
    }
}

static inline void
_updateload() {
    const struct sc_node* node = sc_node_get(HNODE_ID(NODE_GATELOAD, 0));
    if (node) {
        UM_DEFFIX(UM_UPDATELOAD, load);
        load->value = sc_gate_usedclient();
        sc_debug("update load %d", load->value);
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
        c = sc_gate_getclient(id); 
        assert(c);
        _read(self, c, nm);
        }
        break;
    case NETE_ACCEPT:
        // do not forward to handler
        c = sc_gate_acceptclient(id);
        sc_debug("accept %d", id);
        if (!self->need_verify && c) {
            sc_gate_loginclient(c);
        }
        break;
    case NETE_SOCKERR: {
        c = sc_gate_getclient(id);
        assert(c);
        if (c->status == GATE_CLIENT_LOGINED) {
            struct gate_message gm;
            gm.c = c;
            gm.msg = nm;
            service_notify_net(self->handler, (void*)&gm);
        }
        sc_gate_disconnclient(c, true);
        }
        break;
    case NETE_WRIDONECLOSE: {
        // donot forward to handler
        c = sc_gate_getclient(id);
        assert(c);
        sc_gate_disconnclient(c, true); 
        }
        break;
    }
}

void
gate_time(struct service* s) {
    struct gate* self = SERVICE_SELF; 
    uint64_t now = sc_timer_now();
    
    struct gate_client* p = sc_gate_firstclient();
    struct gate_client* c;
    int max = sc_gate_maxclient();
    int i;
    for (i=0; i<max; ++i) {
        c = &p[i];
        switch (c->status) {
        case GATE_CLIENT_CONNECTED:
            if (now - c->active_time > 10*1000) {
                sc_debug("login timeout");
                sc_gate_disconnclient(c, true);
            }
            break;
        case GATE_CLIENT_LOGINED:
            if (self->livetime > 0 &&
                self->livetime < now - c->active_time) {
                sc_debug("heartbeat timeout");
                struct gate_message gm;
                struct net_message nm;
                nm.type = NETE_TIMEOUT;
                gm.c = c;
                gm.msg = &nm;
                service_notify_net(self->handler, (void*)&gm);
                sc_gate_disconnclient(c, true);
            }
            break;
        case GATE_CLIENT_LOGOUTED:
            if (now - c->active_time > 5*1000) {
                sc_debug("logout timeout");
                sc_gate_disconnclient(c, true);
            }
            break;
        default:
            break;
        }
    }
}
