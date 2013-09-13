#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host_timer.h"
#include "host_dispatcher.h"
#include "host.h"
#include "hashid.h"
#include "user_message.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

struct _client {
    int id;
    int connid;
    bool connected;
};

struct _clients {
    struct hashid hash;
    struct _client* clients;
    int nclient;
    int nconnect;
    int query;
    int query_send;
    int query_done;
    uint64_t start;
    uint64_t end;
};

struct _clients*
client_create() {
    struct _clients* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
client_free(struct _clients* self) {
    if (self == NULL)
        return;

    hashid_fini(&self->hash);
    free(self->clients);
    free(self);
}

int
client_init(struct service* s) {
    struct _clients* self = SERVICE_SELF;

    self->query = host_getint("client_query", 0); 
    self->query_send = 0;
    self->query_done = 0;

    int max = host_getint("client_max", 0); 
    int port = host_getint("gate_port", 0);
    const char* ip = host_getstr("gate_ip", "");

    int count = max;
    int hashcap = 1;
    while (hashcap < self->nclient)
        hashcap *= 2;

    hashid_init(&self->hash, max, hashcap);
    self->clients = malloc(sizeof(struct _client) * max);
    memset(self->clients, 0, sizeof(struct _client) * max);
    self->nclient = max;
    self->nconnect = count;
    self->start = self->end = 0;
    host_timer_register(s->serviceid, 1000);

    int n = 0;
    int i;
    for (i=0; i<max; ++i) { 
        if (host_net_connect(ip, port, 1, s->serviceid) < 0) {
            host_error(host_net_error());
            count--;
        }
        if (++n > 64) {
            //usleep(50000);
            n = 0;
        }
    }
    host_info("connect client count: %d", count);
    if (count == 0) {
        return 1;
    }
    host_dispatcher_subscribe(s->serviceid, 100);
    return 0;
}

static struct _client*
_create_client(struct _clients* self, struct net_message* nm) {
    /*if (hashid_full(&self->hash)) {
        host_net_close_socket(nm->connid);
        host_debug("client is full"); 
        return NULL; // full
    }
    int hash = hashid_hash(&self->hash, nm->connid);
    if (hash < 0) {
        return NULL; // error
    }

    struct _client* c = &self->clients[hash];
    */
    //host_error("%d ", nm->connid);
    struct _client* c = &self->clients[nm->connid];
    c->id = nm->connid;
    c->connid = nm->connid;
    c->connected = true;
    host_net_subscribe(nm->connid, false, true);
    return c;
}

static void
_free_client(struct _clients* self, struct _client* c) {
    //hashid_remove(&self->hash, c->connid);
    c->connected = false;
}

static inline struct _client*
_find_client(struct _clients* self, int id) {
    /*
    int hash = hashid_find(&self->hash, id);
    return &self->clients[hash];
    */
    return &self->clients[id];
}

static void
_send_one(struct _clients* self, int id) {
    const int sz = 10;
    UM_DEF(um, sz);
    um->msgid = 100;
    //memcpy(tm.data, "ping pong!", sizeof(tm.data));
    host_net_subscribe(id, false, true);
    UM_SEND(id, um, sz);
    
    self->query_send++;
}

void
_handle_message(struct _clients* self, struct _client* c, struct user_message* um) {
    self->query_done++;
    //assert(self->query_done == self->query_send);
    //host_info("query done one: %d", self->query_done);
    if (self->query_done == self->query) {
        self->end = host_timer_now();
        uint64_t elapsed = self->end - self->start;
        float qps = self->query_done/(elapsed*0.001f);
        host_info("query done : %d, use time: %d, qps: %f", 
        self->query_done, elapsed, qps);
    }
    _send_one(self, c->connid);
}

void
_read(struct _clients* self, int id) {
    struct _client* c = _find_client(self, id);

    const char* error;
    struct user_message* um = UM_READ(id, &error);
    while (um) { 
        _handle_message(self, c, um);
        host_net_dropread(id);
        um = UM_READ(id, &error);
    }
    if (!NET_OK(error)) {
        _free_client(self, c);
    }
}

static void
_write_done(struct _clients* self, int id) {
    //host_info("write done %d", id);
    host_net_subscribe(id, true, false);
}

void
client_usermsg(struct service* s, int id, void* msg, int sz) {
    struct _clients* self = SERVICE_SELF;
    struct _client* c = _find_client(self, id);
    _handle_message(self, c, msg);
}

void
client_net(struct service* s, struct net_message* nm) {
    struct _clients* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        //_read(self, nm->connid);
        break;
    case NETE_WRITEDONE:
        _write_done(self, nm->connid);
        break;
    case NETE_CONNECT:
        _create_client(self, nm);
        break;
    case NETE_SOCKERR:
        host_error("error: %s", host_net_error());
        break;
    }
}

void
client_time(struct service* s) {
    struct _clients* self = SERVICE_SELF;
    //if (self->query_send >= self->query) {
        //host_info("query send : %d", self->query_send);
        //return;
    //}
    if (self->query_send > 0) {
        return;
    }
    struct _client* c = NULL;
    int n = 0;
    int i;
    for (i=0; i<self->nclient; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            ++n;
        }
    }
    if (n != self->nconnect)
        return;
   
    host_error("start clients %d", n);
    self->start = host_timer_now();
        
    for (i=0; i<self->nclient; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            _send_one(self, c->connid);
        }
    }
}
