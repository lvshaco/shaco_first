#include "sh.h"
#include "freeid.h"
#include "msg_client.h"
#include "msg_server.h"

/*
 * control the client connect, login, heartbeat, and logout, 
 * the handler module only focus on logined client
 */

#define S_FREE        0
#define S_CONNECTED   1
#define S_LOGINED     2
#define S_LOGOUTED    3

struct client {
    int connid;
    int status;
    uint64_t active_time;
};

struct gate {
    int  handler;
    int  load_handle;
    int  livetime;
    bool need_verify;

    int  cmax;
    int  used;
    struct freeid fi;
    struct client* p;
};

static inline void
update_load(struct module *s) {
    struct gate *self = MODULE_SELF;
    if (self->load_handle != -1) {
        UM_DEFFIX(UM_UPDATELOAD, load);
        load->value = self->used;
        sh_trace("Load value %d update to %0x", load->value, self->load_handle);
        sh_module_send(MODULE_ID, self->load_handle, MT_UM, load, sizeof(*load));
    }
}

static void
init_clients(struct gate *self, int cmax, int hmax) {
    assert(cmax > 0);
    self->cmax = cmax;
    self->p = malloc(sizeof(self->p[0]) * cmax);
    memset(self->p, 0, sizeof(self->p[0]) * cmax);
    freeid_init(&self->fi, cmax, hmax);
}

static void
free_clients(struct gate *self) {
    freeid_fini(&self->fi);
    free(self->p);
    self->p = NULL;
}

static inline struct client*
accept_client(struct module *s, int connid) {
    struct gate *self = MODULE_SELF;
    assert(connid != -1);
    int id = freeid_alloc(&self->fi, connid);
    if (id == -1) {
        sh_net_close_socket(connid, true);
        return NULL;
    }
    assert(id >= 0 && id < self->cmax);
    struct client* c = &self->p[id];
    assert(c->status == S_FREE);
    c->connid = connid;
    c->status = S_CONNECTED;
    c->active_time = sh_timer_now();
    sh_net_subscribe(connid, true);
    self->used++;

    update_load(s);
    return c;
}

static inline void 
login_client(struct client* c) { 
    if (c->status == S_CONNECTED) {
        c->status = S_LOGINED; 
        c->active_time = sh_timer_now();
    }
}

static bool
dishonnect_client(struct module *s, struct client* c, bool force) {
    struct gate *self = MODULE_SELF;
    if (c->status == S_FREE)
        return true;
    bool closed = sh_net_close_socket(c->connid, force);
    if (closed) {
        int id = freeid_free(&self->fi, c->connid);
        assert(id == (c-self->p));
        c->status = S_FREE;
        c->active_time = 0;
        self->used--;

        update_load(s);
    } else {
        if (c->status != S_LOGOUTED) {
            c->status = S_LOGOUTED;
            c->active_time = sh_timer_now();
        }
    }
    return closed;
}

static inline struct client* 
get_client(struct gate *self, int connid) {
   int id = freeid_find(&self->fi, connid);
   if (id == -1)
       return NULL;
   assert(id >= 0 && id < self->cmax);
   struct client* c = &self->p[id];
   assert(c->connid == connid);
   return c;
}

struct gate*
gate_create() {
    struct gate* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gate_free(struct gate* self) {
    if (self == NULL)
        return;
    free_clients(self);
    free(self);
}

static int
listen_gate(struct module* s) { 
    const char* addr = sh_getstr("gate_ip", "");
    int port = sh_getint("gate_port", 0);
    int wbuffermax = sh_getint("gate_wbuffermax", 0);
    if (addr[0] == '\0')
        return 1;
    if (sh_net_listen(addr, port, wbuffermax, MODULE_ID, 0)) {
        return 1;
    }
    return 0;
}

int
gate_init(struct module* s) {
    struct gate* self = MODULE_SELF;
    
    if (sh_getint("gate_publish", 1)) {
        if (sh_handle_publish(MODULE_NAME, PUB_SER))
            return 1;
    }
    const char* hname = sh_getstr("gate_handler", ""); 
    if (sh_handler(hname, SUB_LOCAL|SUB_REMOTE, &self->handler)) {
        return 1;
    }
    self->load_handle = -1;
    const char *lname = sh_getstr("gate_load", "");
    if (lname[0] != '\0') {
        if (sh_handler(lname, SUB_REMOTE, &self->load_handle)) {
            return 1;
        }
    }
    if (listen_gate(s)) {
        return 1;
    }
    int cmax = sh_getint("gate_clientmax", 0);
    int hmax = sh_getint("sh_connmax", cmax);
    if (cmax <= 0)
        cmax = 1;
    init_clients(self, cmax, hmax);
    sh_info("gate_clientmax = %d", cmax);

    self->need_verify = false;//sh_getint("gate_need_verify", 1);
    
    int live = sh_getint("gate_clientlive", 3);
    self->livetime = live * 1000;
    sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static inline int
handle_client(struct module *s, struct client* c, const void *msg, int sz) {
    if (sz < sizeof(struct UM_BASE) || sz > UM_CLI_MAXSZ) {
        return 1;
    }
    struct gate *self = MODULE_SELF;
    if (c->status == S_LOGINED) {
        c->active_time = sh_timer_now();
    }
    UM_CAST(UM_BASE, um, msg);
    //if (um->msgid != IDUM_HEARTBEAT) {
    if (um->msgid >= IDUM_GATEB && um->msgid <= IDUM_GATEE) {
        UM_DEFWRAP2(UM_GATE, ga, UM_CLI_MAXSZ);
        ga->connid = c->connid;
        memcpy(ga->wrap, msg, sz);

        sh_trace("Client %d receive msg: %u", c->connid, um->msgid);
        sh_module_send(MODULE_ID, self->handler, MT_UM, ga, sizeof(*ga)+sz);
    }
    return 0;
}

static void
read(struct module* s, struct client* c, struct net_message* nm) {
    int id = nm->connid;
    int step = 0;
    int drop = 1;
    int err;
    for (;;) {
        err = 0; 
        struct mread_buffer buf;
        int nread = sh_net_read(id, drop==0, &buf, &err);
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
            if (handle_client(s, c, buf.ptr+2, sz-2)) {
                err = NET_ERR_MSG;
                break;
            }
            buf.ptr += sz;
            buf.sz  -= sz;
            if (++step > 10) {
                sh_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (err) {
            sh_net_close_socket(id, true);
            goto errout;
        }
        drop = nread - buf.sz;
        sh_net_dropread(id, drop);       
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    module_net(nm->ud, nm);
}

static inline void
send_to_client(struct client *cl, void *data, int sz) {
    uint8_t *tmp = malloc(sz+2);
    sh_to_littleendian16(sz, tmp);
    memcpy(tmp+2, data, sz);
    sh_net_send(cl->connid, tmp, sz+2);
}

void
gate_main(struct module* s, int session, int source, int type, const void *msg, int sz) {
    struct gate* self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_GATE: {
            UM_CAST(UM_GATE, ga, msg); 
            UM_CAST(UM_BASE, sub, ga->wrap);
            int connid = ga->connid; 
            struct client *cl = get_client(self, connid);
            if (cl == NULL) {
                sh_trace("Client %d send %d sz %d, but closed", 
                        connid, sub->msgid, sz-(int)sizeof(*ga));
                return;
            }
            sh_trace("Client %d send %d sz %d", 
                        connid, sub->msgid, sz-(int)sizeof(*ga));
            switch (sub->msgid) {
            case IDUM_LOGOUT: {
                UM_CAST(UM_LOGOUT, lo, sub);
                if (lo->err == SERR_OK) {
                    dishonnect_client(s, cl, true);
                } else {
                    send_to_client(cl, lo, sizeof(*lo));
                    dishonnect_client(s, cl, false);
                }
                break;
                }
            default:
                send_to_client(cl, ga->wrap, sz-sizeof(*ga));
                break;
            }
            break;
            } 
        }
        break;
        }
    }
}

void
gate_net(struct module* s, struct net_message* nm) {
    struct gate* self = MODULE_SELF;
    struct client* c;
    int id = nm->connid;
    switch (nm->type) {
    case NETE_READ: {
        c = get_client(self, id); 
        assert(c);
        read(s, c, nm);
        }
        break;
    case NETE_ACCEPT:
        sh_trace("Client %d accepted", id);
        // do not forward to handler
        c = accept_client(s, id); 
        if (!self->need_verify && c) {
            login_client(c);
        }
        break;
    case NETE_SOCKERR: {
        sh_trace("Client %d sockerr dishonnect %d", id, nm->error);
        c = get_client(self, id);
        assert(c);
        if (c->status == S_LOGINED) { 
            UM_DEFWRAP(UM_GATE, ga, UM_NETDISCONN, nd);
            ga->connid = id;
            nd->type = NETE_SOCKERR;
            nd->err  = nm->error;
            sh_module_send(MODULE_ID, self->handler, MT_UM, ga, sizeof(*ga) + sizeof(*nd));
        }
        dishonnect_client(s, c, true);
        }
        break;
    case NETE_WRIDONECLOSE: {
        // donot forward to handler
        c = get_client(self, id);
        assert(c);
        sh_trace("Client %d writedone close", id);
        dishonnect_client(s, c, true); 
        }
        break;
    }
}

void
gate_time(struct module* s) {
    struct gate* self = MODULE_SELF; 
    uint64_t now = sh_timer_now();
    
    int i;
    for (i=0; i<self->cmax; ++i) {
        struct client *c = &self->p[i];
        switch (c->status) {
        case S_CONNECTED:
            if (now - c->active_time > 10*1000) {
                sh_trace("Client %d login timeout", c->connid);
                dishonnect_client(s, c, true);
            }
            break;
        case S_LOGINED:
            if (self->livetime > 0 &&
                self->livetime < now - c->active_time) {
                sh_trace("Client %d heartbeat timeout", c->connid);
                
                UM_DEFWRAP(UM_GATE, ga, UM_NETDISCONN, nd);
                ga->connid = c->connid;
                nd->type = NETE_TIMEOUT;
                nd->err  = 0;
                sh_module_send(MODULE_ID, self->handler, MT_UM, ga, sizeof(*ga) + sizeof(*nd));
                
                dishonnect_client(s, c, true);
            }
            break;
        case S_LOGOUTED:
            if (now - c->active_time > 5*1000) {
                sh_trace("Client %d logout timeout", c->connid);
                dishonnect_client(s, c, true);
            }
            break;
        default:
            break;
        }
    }
}