#include "host_service.h"
#include "host_node.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "node_type.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>

struct centerc {
    int nsub;
    int subs[NODE_TYPE_MAX];
};

struct centerc* 
centerc_create() {
    struct centerc* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
centerc_free(struct centerc* self) {
    free(self);
}

static int
_connect_center(struct service* s) {
    const char* addr = host_getstr("center_ip", "");
    int port = host_getint("center_port", 0);
    host_info("connect to %s:%u ...", addr, port);
    if (host_net_connect(addr, port, true, s->serviceid, 0)) { 
        return 1;
    }
    return 0;
}

static int
_fill_subs(struct centerc* self) {
    const char* str = host_getstr("node_sub", "");
    if (str[0] == '\0')
        return 0;
    
    char tmp[strlen(str)+1];
    strcpy(tmp, str);
    
    int n = 0;
    char* saveptr;
    char* one = strtok_r(tmp, ",", &saveptr);
    while (one) {
        int tid = host_node_typeid(one);
        if (tid == -1) {
            host_error("subscribe unknown node:%s", one);
            return 1;
        }
        self->subs[n++] = tid;
        one = strtok_r(NULL, ",", &saveptr);
    }
    self->nsub = n;
    return 0;
}

int
centerc_init(struct service* s) {
    struct centerc* self = SERVICE_SELF;
    if (_connect_center(s))
        return 1;
  
    if (_fill_subs(self))
        return 1;
    
    return 0;
}

static void
_reg_request(int id) {
    struct host_node* me = host_me();
    UM_DEFFIX(UM_NODEREG, reg);
    reg->addr = me->addr;
    reg->port = me->port;
    reg->gaddr = me->gaddr;
    reg->gport = me->gport;
    UM_SEND(id, reg, sizeof(*reg));
}

static void
_sub_request(struct centerc* self, int id) {
    UM_DEFVAR(UM_NODESUBS, req);
    int i;
    for (i=0; i<self->nsub; ++i) {
        req->subs[i] = self->subs[i];
    }
    req->n = self->nsub;
    UM_SEND(id, req, UM_NODESUBS_size(req));
}

void
centerc_service(struct service* s, struct service_message* sm) {
    struct centerc* self = SERVICE_SELF;
    int center_connid = sm->sessionid;
    _sub_request(self, center_connid);
}

void
centerc_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    case NETE_CONNECT:
        host_info("connect to center ok");
        host_net_subscribe(nm->connid, true);
        _reg_request(nm->connid);
        break;
    case NETE_CONNERR:
        host_error("connect to center fail: %s", host_net_error(nm->error));
        break;
    case NETE_SOCKERR:
        host_error("center disconnect: %s", host_net_error(nm->error));
        host_node_disconnect(nm->connid);
        break;
    }
}
