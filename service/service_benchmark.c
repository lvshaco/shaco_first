#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_dispatcher.h"
#include "host.h"
#include "hashid.h"
#include "freeid.h"
#include "message_reader.h"
#include "user_message.h"
#include "client_type.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

struct client {
    int connid;
    bool connected;
};

struct benchmark {
    struct freeid fi;
    struct client* clients;
    int max;
    int connected;
    int query;
    int query_first;
    int query_send;
    int query_done;
    int packetsz;
    uint64_t start;
    uint64_t end; 
};

struct benchmark*
benchmark_create() {
    struct benchmark* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
benchmark_free(struct benchmark* self) {
    if (self == NULL)
        return;

    freeid_fini(&self->fi);
    free(self->clients);
    free(self);
}

static int
_connect(struct service* s) {
    struct benchmark* self = SERVICE_SELF;
    const char* ip = host_getstr("echo_ip", "");
    int port = host_getint("echo_port", 0);
    int count = 0;
    int i;
    for (i=0; i<self->max; ++i) { 
        if (host_net_connect(ip, port, 1, s->serviceid, CLI_GAME) == 0) {
            count++;
        }
    }
    host_info("connected client count: %d", count);
    return count;
}

static void
_send_one(struct benchmark* self, int id) {
    int sz = self->packetsz;
    UM_DEF(um, sz);
    memset(um, 0, sz);
    um->msgid = 100;
    //memcpy(tm.data, "ping pong!", sizeof(tm.data));
    host_net_subscribe(id, true);
    UM_SENDTOSVR(id, um, sz);
    self->query_send++;
}

static void
_start(struct benchmark* self) {
    self->start = host_timer_now();
    struct client* c;
    int i;
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            if (self->query_first > 0) {
                int n;
                for (n=0; n<self->query_first; ++n) {
                    _send_one(self, c->connid);
                }
                host_info("start send %d", n);
            } else {
                _send_one(self, c->connid);
            }
        }
    }
}

int
benchmark_init(struct service* s) {
    struct benchmark* self = SERVICE_SELF;

    self->query = host_getint("benchmark_query", 0); 
    self->query_first = host_getint("benchmark_query_first", 0);
    self->query_send = 0;
    self->query_done = 0;
    int sz = host_getint("benchmark_packet_size", 10);
    if (sz < sizeof(struct UM_BASE))
        sz = sizeof(struct UM_BASE);
    self->packetsz = sz;
    int hmax = host_getint("host_connmax", 0);
    int cmax = host_getint("benchmark_client_max", 0); 
    
    self->max = cmax;
    self->clients = malloc(sizeof(struct client) * cmax);
    memset(self->clients, 0, sizeof(struct client) * cmax);
    freeid_init(&self->fi, cmax, hmax);
    
    self->start = 0;
    self->end = 0;

    self->connected = _connect(s);
    if (self->connected == 0) {
        return 1;
    }
    //_start(self);
    host_timer_register(s->serviceid, 1000);
    return 0;
}

static inline struct client*
_getclient(struct benchmark* self, int id) {
    if (id >= 0 && id < self->max) {
        struct client* c = &self->clients[id];
        if (c->connected)
            return c;
    }
    return NULL;
}

static inline void
_handlemsg(struct benchmark* self, struct client* c, struct UM_BASE* um) {
    self->query_done++;
    if (self->query_done == self->query) {
        self->end = host_timer_now();
        uint64_t elapsed = self->end - self->start;
        if (elapsed == 0) elapsed = 1;
        float qps = self->query_done/(elapsed*0.001f);
        host_info("clients: %d, packetsz: %d, query done: %d, use time: %d, qps: %f", 
        self->connected, self->packetsz, self->query_done, (int)elapsed, qps);
        self->start = self->end;
        self->query_done = 0;
    }
    _send_one(self, c->connid);
}

static void
_read(struct benchmark* self, struct net_message* nm) {
    int id = nm->connid;
    struct client* c = _getclient(self, id);
    assert(c);
    assert(c->connid == id);
    int n = 0;
    struct UM_BASE* um;
    while ((um = _message_read_one(nm, UM_SKIP)) != NULL) {
        if (um->msgsz > UM_CLIMAX) {
            host_net_close_socket(nm->connid, true);
            nm->type = NETE_SOCKERR;
            nm->error = -2;
            service_notify_net(nm->ud, nm);
            break;
        }
        _handlemsg(self, c, um);
        host_net_dropread(nm->connid, UM_SKIP);
        if (++n > 10)
            break;
    }

}

static void
_onconnect(struct benchmark* self, int connid) {
    int id = freeid_alloc(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    assert(!c->connected);
    c->connected = true;
    c->connid = connid;
    //c->active_time = host_timer_now();
    
    host_net_subscribe(connid, false);
}

static void
_ondisconnect(struct benchmark* self, int connid) {
    int id = freeid_free(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    c->connected = false;
}
/*
static void
_disconnect(struct benchmark* self, int id, const char* error) {
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id]; 
    assert(c->connected);
    assert(c->connid >= 0);
    int tmp = freeid_free(&self->fi, c->connid);
    assert(tmp == id);
    host_net_close_socket(c->connid);
    c->connected = false;
}
*/

void
benchmark_net(struct service* s, struct net_message* nm) {
    struct benchmark* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm);
        break;
    case NETE_CONNECT:
        _onconnect(self, nm->connid);
        break;
    case NETE_SOCKERR:
        _ondisconnect(self, nm->connid);
        break;
    }
}

void
benchmark_time(struct service* s) {
    struct benchmark* self = SERVICE_SELF;
    if (self->query_send > 0) {
        return;
    }
    struct client* c = NULL;
    int n = 0;
    int i;
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            ++n;
        }
    }
    if (n != self->connected)
        return;

    _start(self);
}
