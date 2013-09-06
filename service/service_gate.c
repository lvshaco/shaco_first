#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "hashid.h"
#include "user_message.h"
#include <stdlib.h>

struct _client {
    int id;
    int connid;
};

struct _gate {
    struct hashid hash;
    struct _client* clients;    
};

struct _gate*
gate_create() {
    struct _gate* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gate_free(struct _gate* self) {
    if (self == NULL)
        return;

    hashid_fini(&self->hash);
    free(self->clients);
    free(self);
}

int
gate_init(struct service* s) {
    struct _gate* self = SERVICE_SELF;
    
    int max = host_getint("gate_client_max", 0);
    int port = host_getint("gate_port", 0);
    const char* ip = host_getstr("gate_ip", "");
    
    int hashcap = 1;
    while (hashcap < max)
        hashcap *= 2;

    hashid_init(&self->hash, max, hashcap);
    self->clients = malloc(sizeof(struct _client) * max);
    memset(self->clients, 0, sizeof(struct _client) * max);

    if (host_net_listen(ip, port, s->serviceid)) {
        host_error("listen fail: %s", host_net_error());
        return 1;
    }
    host_info("listen on %s:%d max_client=%d", ip, port, max);
    return 0;
}

static struct _client*
_create_client(struct _gate* self, struct net_message* nm) {
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
    host_net_subscribe(nm->connid, true, true);
    host_info("new client: %d", nm->connid);
    return c;
}

static void
_free_client(struct _gate* self, struct _client* c) {
    hashid_remove(&self->hash, c->connid);
}

static inline struct _client*
_find_client(struct _gate* self, int id) {
    int hash = hashid_find(&self->hash, id);
    return &self->clients[hash];
}

void
_handle_message(struct _client* c, struct user_message* um) {
    host_net_send(c->connid, um, sizeof(*um) + um->sz);
}

void
_read(struct _gate* self, int id) {
    struct _client* c = _find_client(self, id);

    const char* error;
    struct user_message* um = user_message_read(id, &error);
    while (um) {
        _handle_message(c, um);
        host_net_dropread(id);
        //host_info("read one");
        um = user_message_read(id, &error);
    }
    if (!NET_OK(error)) {
        _free_client(self, c);
    }
}

void
gate_net(struct service* s, struct net_message* nm) {
    struct _gate* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm->connid);
        break;
    case NETE_ACCEPT:
        _create_client(self, nm);
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

void
gate_time(struct service* s) {

}

/*
void
gate_service(struct service* s, struct service_message* sm) {

}
*/
