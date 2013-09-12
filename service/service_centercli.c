#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "hashid.h"
#include "user_message.h"
#include <stdlib.h>
/*
#define UNCONNECT 0
#define CONNECTING 1
#define CONNECTED 2

struct _centercli {
    int connection;
    int status;
};

struct _centercli*
centercli_create() {
    struct _centercli* self = malloc(sizeof(*self));
    self->connection = -1;
    self->status = UNCONNECT;
    return self;
}

void
centercli_free(struct _centercli* self) {
    free(self);
} 

static int
_connect_center(struct serviec* s, bool block) {
    struct _centercli* self = SERVICE_SELF;

    const char* ip = host_getstr("center_ip", "");
    int port = host_getint("center_port", 0);

    if (host_net_connect(ip, port, block, s->serviceid) < 0) {
        host_error("connect center fail: %s", host_net_error());
        return 1;
    }
    self->status = CONNECTING;
    return 0;
}

int
centercli_init(struct service* s) {
    host_timer_register(s->serviceid, 3000);
    return _connect_center(s, true);
}

static
_reg(struct _centercli* self) {
    UM_DEF(um, 128);
    um->sz = snprintf(um->data, sizeof(um->data), "REG name=%s ip=%u port=%u connect=%s",
            host_getstr("host_name", ""),
            inet_addr(host_getstr("host_ip", "")),
            host_getint("host_port", 0),
            host_getstr("host_connect", ""));
    um->sz += 1;
    host_net_send(self->connection, um, UM_SIZE(um));
}

static void
_handle_message(struct _centercli* self, struct user_message* um) {
    um->data[um->sz] = '\0';
    if (memcmp("CON", um->data, 3) == 0) {
        char name[16];
        uint32_t ip;
        int port; 
        sscanf(um->data+4, "name=%s ip=%u port=%u", 
                name, ip, &port);
        // todo connect remote
    }
}

static
_read(struct _centercli* self, int id) {
    if (self->connection != id) {
        host_error("centercli connection dismatch(self:%d, input:%d", self->connection, id);
        return;
    }
    const char* error;
    struct user_message* um = user_message_read(id, &error);
    while (um) {
        _handle_message(self, um);
        host_net_dropread(id);
        um = user_message_read(id, &error);
    }
    if (!NET_OK(error)) {
        self->connection = -1;
        self->status = UNCONNECT;
    }
}

void
centercli_net(struct service* s, struct net_message* nm) {
    struct _centercli* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        break;
    case NETE_CONNECT: {
        self->connection = nm->connid;
        self->status = CONNECTED;
        _reg(self);
        break;
        }
    case NETE_CONNECTERR: {
        self->connection = -1;
        self->status = UNCONNECT;
        host_error("connect center fail: %s", host_net_error());
        break;
        }
    }
}

void
centercli_time(struct service* s) {
    struct _centercli* self = SERVICE_SELF;
    if (self->status == UNCONNECT) {
        _connect_center(s, false);
    }
}
*/
