#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host_timer.h"
#include "host.h"
#include "hashid.h"
#include "user_message.h"
#include <stdlib.h>

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
    host_timer_register(s->serviceid, 1);

    int i;
    for (i=0; i<max; ++i) {
        if (host_net_connect(ip, port, 1, s->serviceid) < 0) {
            host_error(host_net_error());
            count--;
        }
    }
    host_info("connect client count: %d", count);
    if (count == 0) {
        return 1;
    }
    return 0;
}

static struct _client*
_create_client(struct _clients* self, struct net_message* nm) {
    if (hashid_full(&self->hash)) {
        host_net_close_socket(nm->connid);
        host_debug("client is full"); 
        return NULL; // full
    }
    int hash = hashid_hash(&self->hash, nm->connid);
    if (hash < 0) {
        return NULL; // error
    }

    struct _client* c = &self->clients[hash];
    c->id = nm->connid;
    c->connid = nm->connid;
    c->connected = true;
    host_net_subscribe(nm->connid, true, true);
    return c;
}

static void
_free_client(struct _clients* self, struct _client* c) {
    hashid_remove(&self->hash, c->connid);
    c->connected = false;
}

static inline struct _client*
_find_client(struct _clients* self, int id) {
    int hash = hashid_find(&self->hash, id);
    return &self->clients[hash];
}

void
_handle_message(struct _client* c, struct user_message* um) {
}

void
_read(struct _clients* self, int id) {
    struct _client* c = _find_client(self, id);

    const char* error;
    struct user_message* um = user_message_read(id, &error);
    while (um) {
        _handle_message(c, um);
        host_net_dropread(id);

        self->query_done++;
        //host_info("query done one: %d", self->query_done);
        if (self->query_done >= self->query) {
            self->end = host_timer_now();
            host_info("query done : %d, use time: %d", 
            self->query_done, self->end-self->start);
        }
        um = user_message_read(id, &error);
    }
    if (!NET_OK(error)) {
        _free_client(self, c);
    }
}

void
client_net(struct service* s, struct net_message* nm) {
    struct _clients* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm->connid);
        break;
    case NETE_CONNECT:
        _create_client(self, nm);
        break;
    case NETE_CONNECTERR: {
        const char* error = host_net_error();
        host_debug("connect failed: %s", error);
        break;
        }
    }
}

static void
_gen_message(struct test_message* tm) {
    tm->sz = sizeof(*tm) - sizeof(struct user_message);
    memcpy(tm->data, "ping pong!", sizeof(tm->data));
}

void
client_time(struct service* s) {
    struct _clients* self = SERVICE_SELF;
    if (self->query_send >= self->query) {
        //host_info("query send : %d", self->query_send);
        return;
    }
    if (self->query_send == 0) {
        self->start = host_timer_now();
    }
    struct _client* c = NULL;
    int i;
    for (i=0; i<self->nclient; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            struct test_message tm;
            _gen_message(&tm);
            host_net_send(c->connid, &tm, sizeof(tm));
            self->query_send++;
            //host_info("query send one : %d", self->query_send);
        }
    }
}
