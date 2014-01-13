#include "sc_service.h"
#include "sc_node.h"
#include "sc_env.h"
#include "sc_net.h"
#include "sc_log.h"
#include "sc_timer.h"
#include "sc.h"
#include "sc_gate.h"
#include "sh_util.h"
#include "user_message.h"
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
    int load_handle;
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
    if (sc_net_listen(addr, port, wbuffermax, s->serviceid, 0)) {
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
    self->need_load = sc_getint("gate_load", 0);
    if (self->need_load) {
        const char *lname = sc_getstr("gate_load", "");
        if (sc_handler(lname, &self->load_handle)) {
            return 1;
        }
    }
    if (_listen(s))
        return 1;
    int cmax = sc_getint("gate_clientmax", 0);
    int hmax = sc_getint("sc_connmax", cmax);
    if (sc_gate_prepare(cmax, hmax)) {
        return 1;
    }
    sc_info("gate_clientmax = %d", cmax);

    self->need_verify = sc_getint("gate_verify", 1);
    
    int live = sc_getint("gate_clientlive", 3);
    self->livetime = live * 1000;
    sc_timer_register(s->serviceid, 1000);
    return 0;
}

static inline int
_handlemsg(struct service *s, struct gate_client* c, const void *msg, int sz) {
    if (sz < 2 || sz > UM_CLI_MAXSZ) {
        return 1; // least 2 bytes for msgid
    }
    struct gate *self = SERVICE_SELF;
    if (c->status == GATE_CLIENT_LOGINED) {
        c->active_time = sc_timer_now();
    }
    UM_CAST(UM_BASE, um, msg);
    if (um->msgid != IDUM_HEARTBEAT) {
        UM_DEFVAR2(UM_GATE, gm, UM_CLI_MAXSZ+sizeof(struct UM_GATE));
        gm->connid = c->connid;
        memcpy(gm->wrap, msg, sz);

        sc_debug("Receive msg:%u", um->msgid);
        sh_service_send(SERVICE_ID, self->handler, MT_UM, gm, sizeof(*gm)+sz);
    }
    return 0;
}

static void
_read(struct service* s, struct gate_client* c, struct net_message* nm) {
    int id = nm->connid;
    int step = 0;
    int drop = 1;
    int err;
    for (;;) {
        err = 0; 
        struct mread_buffer buf;
        int nread = sc_net_read(id, drop==0, &buf, &err);
        if (nread <= 0) {
            if (!err)
                return;
            else
                goto errout;
        }
        for (;;) {
            if (buf.sz < 2) {
                break;
            }
            uint16_t sz = sh_from_littleendian16((uint8_t*)buf.ptr) + 2;
            if (buf.sz < sz) {
                break;
            }
            buf.ptr += sz;
            buf.sz  -= sz;
            if (_handlemsg(s, c, buf.ptr+2, sz-2)) {
                err = NET_ERR_MSG;
                break;
            }
            if (++step > 10) {
                sc_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (err) {
            sc_net_close_socket(id, true);
            goto errout;
        }
        drop = nread - buf.sz;
        sc_net_dropread(id, drop);       
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    service_net(nm->ud, nm);
}

static inline void
_updateload(struct service *s) {
    struct gate *self = SERVICE_SELF;
    if (self->load_handle != -1) {
        UM_DEFFIX(UM_UPDATELOAD, load);
        load->value = sc_gate_usedclient();
        sc_debug("update load %d", load->value);
        sh_service_send(SERVICE_ID, self->load_handle, MT_UM, load, sizeof(*load));
    }
}

void
gate_main(struct service* s, int session, int source, int type, const void *msg, int sz) {
    struct gate* self = SERVICE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_GATE, g, msg);
        UM_CAST(UM_BASE, sub, g->wrap);
        switch (sub->msgid) {
        case IDUM_CLOSECONN: {
            struct gate_client *c = sc_gate_getclient(g->connid);
            if (c) {
                UM_CAST(UM_CLOSECONN, cc, sub);
                sc_gate_disconnclient(c, cc->force);
            }
            }
            break;
        default:
            sc_net_send(g->connid, g->wrap, sz-sizeof(*g));
            break;
        }
        break;
        }
    case MT_GATE: {
        if (!self->need_load) {
            return;
        }
        int event = session;
        if (event == GATE_EVENT_ONDISCONN ||
            event == GATE_EVENT_ONACCEPT) {
            _updateload(s);
        }
        break;
        }
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
        _read(s, c, nm);
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
            UM_DEFVAR2(UM_GATE, gm, UM_CLI_MAXSZ+sizeof(struct UM_GATE));
            gm->connid = id;
            UD_CAST(UM_NETDISCONN, disconn, gm->wrap);
            disconn->err = NETE_SOCKERR;
            sh_service_send(SERVICE_ID, self->handler, MT_UM, gm, sizeof(*gm) + sizeof(*disconn));
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
                UM_DEFVAR2(UM_GATE, gm, UM_CLI_MAXSZ+sizeof(struct UM_GATE));
                gm->connid = c->connid;
                UD_CAST(UM_NETDISCONN, disconn, gm->wrap);
                disconn->err = NETE_TIMEOUT;
                sh_service_send(SERVICE_ID, self->handler, MT_UM, 
                        gm, sizeof(*gm) + sizeof(*disconn));

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
