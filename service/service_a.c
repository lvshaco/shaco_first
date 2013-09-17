#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "hashid.h"
#include "user_message.h"
#include <stdlib.h>
#include <stdio.h>

struct _a {
    int connid;
    bool connected;
};

struct _a*
a_create() {
    struct _a* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
a_free(struct _a* self) {
    if (self == NULL)
        return;
    free(self);
}

int
a_init(struct service* s) {
    struct _a* self = SERVICE_SELF;
    self->connid = -1;
    self->connected = false; 

    int port = host_getint("host_port", 0);
    const char* ip = host_getstr("host_ip", "");
    
    if (host_net_listen(ip, port, s->serviceid, 0)) {
        return 1;
    }
    return 0;
}
/*
void
_handle_message(struct _a* self, struct UM_base* um) {
    UM_SEND(self->connid, um, um->msgsz);
}

void
_read(struct _a* self, int id) {
    const char* error;
    struct UM_base* um = UM_READ(id, &error);
    while (um) {
        _handle_message(self, um);
        host_net_dropread(id);
        //host_info("read one");
        um = UM_READ(id, &error);
    }
    if (!NET_OK(error)) {
        host_error("remote disconnect");
    }
}

void
a_net(struct service* s, struct net_message* nm) {
    struct _a* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm->connid);
        break;
    case NETE_ACCEPT:
        host_net_subscribe(nm->connid, true, false);
        break;
    case NETE_CONNECT:
        self->connected = true;
        self->connid = nm->connid;
        host_net_subscribe(self->connid, false, false);
        break;
    }
}

void
a_time(struct service* s) {
    struct _a* self= SERVICE_SELF;
    if (!self->connected) {
        const char* ip = host_getstr("remote_ip", "");
        int port = host_getint("host_port", 0);
        host_net_connect(ip, port, 1, s->serviceid, 0);
    }
}

void
a_service(struct service* s, struct service_message* sm) {

}
*/
