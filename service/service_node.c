#include "host_service.h"
#include "host.h"
#include "host_log.h"
#include "host_net.h"
#include "host_dispatcher.h"
#include "host_node.h"
#include "node_type.h"
#include "user_message.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>

struct node {
    bool iscenter;
    int center_or_cli_service;
};

struct node*
node_create() {
    struct node* self = malloc(sizeof(*self));
    self->center_or_cli_service = SERVICE_INVALID;
    return self;
}

void
node_free(struct node* self) {
    free(self);
}

static void
_mynode(struct host_node* node) {
    int tid = host_node_typeid(host_getstr("node_type", ""));
    int sid = host_getint("node_sid", 0);
    node->id   = HNODE_ID(tid, sid);
    node->addr = inet_addr(host_getstr("node_ip", "0"));
    node->port = host_getint("node_port", 0);
    node->gaddr = inet_addr(host_getstr("gate_ip", "0"));
    node->gport = host_getint("gate_port", 0);
    node->connid = -1;
}

static int
_listen(struct service* s) {
    const char* addr = host_getstr("node_ip", "");
    int port = host_getint("node_port", 0);
    if (addr[0] != '\0' &&
        host_net_listen(addr, port, s->serviceid, 0)) {
        host_error("listen node fail");
        return 1;
    }
    return 0;
}

int
node_init(struct service* s) {
    struct node* self = SERVICE_SELF;
    SUBSCRIBE_MSG(s->serviceid, IDUM_NODEREG);
    SUBSCRIBE_MSG(s->serviceid, IDUM_NODEREGOK);
    SUBSCRIBE_MSG(s->serviceid, IDUM_NODENOTIFY);

    if (host_node_register_types(NODE_NAMES, NODE_TYPE_MAX))
        return 1;
    struct host_node me;
    _mynode(&me);
    if (host_register_me(&me))
        return 1;
    if (_listen(s))
        return 1;

    self->iscenter = HNODE_TID(me.id) == NODE_CENTER;
    const char* tmp = self->iscenter ? "centers" : "centerc";
    self->center_or_cli_service = service_query_id(tmp);
    if (self->center_or_cli_service == -1) {
        host_error("lost %s service", tmp);
        return 1;
    }
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
_reg(struct service* s, int id, struct UM_base* um) {
    struct node* self = SERVICE_SELF;
    UM_CAST(UM_NODEREG, reg, um);
    struct host_node node;
    node.id = reg->nodeid;
    node.addr = reg->addr;
    node.port = reg->port;
    node.gaddr = reg->gaddr;
    node.gport = reg->gport;
    node.connid = id;
    if (host_node_register(&node)) {
        host_net_close_socket(id);
        return; // no need response for fail
    }
    struct host_node* me = host_me();
    UM_DEFFIX(UM_NODEREGOK, ok);
    ok->addr = me->addr;
    ok->port = me->port;
    ok->gaddr = me->gaddr;
    ok->gport = me->gport;
    UM_SEND(id, ok, sizeof(*ok));

    if (self->iscenter) {
        struct service_message sm;
        sm.sessionid = 0;
        sm.source = s->serviceid;
        sm.sz = sizeof(node);
        sm.msg = &node;
        service_notify_service(
        self->center_or_cli_service, &sm);
    }
}

static void
_regok(struct service* s, int id, struct UM_base* um) {
    struct node* self = SERVICE_SELF;
    UM_CAST(UM_NODEREGOK, ok, um);
    struct host_node node;
    node.id = ok->nodeid;
    node.addr = ok->addr;
    node.port = ok->port;
    node.gaddr = ok->gaddr;
    node.gport = ok->gport;
    node.connid = id;
    if (host_node_register(&node)) {
        host_net_close_socket(id);
        return;
    }
    if (!self->iscenter &&
        HNODE_TID(ok->nodeid) == NODE_CENTER) {
        struct service_message sm;
        sm.sessionid = id; // reuse for connid
        sm.source = s->serviceid;
        sm.sz = 0;
        sm.msg = NULL;
        service_notify_service(
        self->center_or_cli_service, &sm);
    }
}

static void
_onnotify(struct service* s, int id, struct UM_base* um) {
    UM_CAST(UM_NODENOTIFY, notify, um);
    struct in_addr in;
    in.s_addr = notify->addr;
    char* saddr = inet_ntoa(in);
    const struct host_node* node = host_node_get(notify->tnodeid);
    if (node == NULL) {
        host_info("connect to %s:%u ...", saddr, notify->port);
        host_net_connect(saddr, notify->port, false, s->serviceid, 0);
    } else {
        // todo address update
    }
}
void
node_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct UM_base* um = msg;
    switch (um->msgid) {
    case IDUM_NODEREG:
        _reg(s, id, um);
        break;
    case IDUM_NODEREGOK:
        _regok(s, id, um);
        break;
    case IDUM_NODENOTIFY:
        _onnotify(s, id, um);
        break;
    }
}

void
node_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    case NETE_ACCEPT:
        host_net_subscribe(nm->connid, true, false);
        break;
    case NETE_CONNECT:
        host_info("connect to node ok");
        host_net_subscribe(nm->connid, true, false);
        _reg_request(nm->connid);
        break;
    case NETE_CONNERR:
        host_error("connect to node fail: %s", host_net_error(nm->error));
        break;
    case NETE_SOCKERR:
        host_error("node disconnect: %s", host_net_error(nm->error));
        host_node_disconnect(nm->connid);
        break;
    }
}
