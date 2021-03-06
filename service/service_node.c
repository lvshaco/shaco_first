#include "sc_service.h"
#include "sc.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_dispatcher.h"
#include "sc_node.h"
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
_mynode(struct sc_node* node) {
    int tid = sc_node_typeid(sc_getstr("node_type", ""));
    int sid = sc_getint("node_sid", 0);
    node->id   = HNODE_ID(tid, sid);
    node->addr = inet_addr(sc_getstr("node_ip", "0"));
    node->port = sc_getint("node_port", 0);
    node->gaddr = inet_addr(sc_getstr("gate_ip", "0"));
    node->gport = sc_getint("gate_port", 0);
    node->connid = -1;
}

static int
_listen(struct service* s) {
    const char* addr = sc_getstr("node_ip", ""); 
    int port = sc_getint("node_port", 0);
    if (addr[0] == '\0')
        return 1;
    if (sc_net_listen(addr, port, 0, s->serviceid, 0)) {
        sc_error("listen node fail");
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

    if (sc_node_register_types(NODE_NAMES, NODE_TYPE_MAX))
        return 1;
    struct sc_node me;
    _mynode(&me);
    if (sc_register_me(&me))
        return 1;
    if (_listen(s))
        return 1;

    self->iscenter = HNODE_TID(me.id) == NODE_CENTER;
    const char* tmp = self->iscenter ? "centers" : "centerc";
    if (sc_handler(tmp, &self->center_or_cli_service))
        return 1;
    return 0;
}

static void
_reg_request(int id) {
    struct sc_node* me = sc_me();
    UM_DEFFIX(UM_NODEREG, reg);
    reg->addr = me->addr;
    reg->port = me->port;
    reg->gaddr = me->gaddr;
    reg->gport = me->gport;
    UM_SEND(id, reg, sizeof(*reg));
}

static void
_reg(struct service* s, int id, struct UM_BASE* um) {
    struct node* self = SERVICE_SELF;
    UM_CAST(UM_NODEREG, reg, um);
    struct sc_node node;
    node.id = reg->nodeid;
    node.addr = reg->addr;
    node.port = reg->port;
    node.gaddr = reg->gaddr;
    node.gport = reg->gport;
    node.connid = id;
    if (sc_node_register(&node)) {
        sc_net_close_socket(id, true);
        return; // no need response for fail
    }
    struct sc_node* me = sc_me();
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
_regok(struct service* s, int id, struct UM_BASE* um) {
    struct node* self = SERVICE_SELF;
    UM_CAST(UM_NODEREGOK, ok, um);
    struct sc_node node;
    node.id = ok->nodeid;
    node.addr = ok->addr;
    node.port = ok->port;
    node.gaddr = ok->gaddr;
    node.gport = ok->gport;
    node.connid = id;
    if (sc_node_register(&node)) {
        sc_net_close_socket(id, true);
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
_onnotify(struct service* s, int id, struct UM_BASE* um) {
    UM_CAST(UM_NODENOTIFY, notify, um);
    struct in_addr in;
    in.s_addr = notify->addr;
    char* saddr = inet_ntoa(in);
    const struct sc_node* node = sc_node_get(notify->tnodeid);
    if (node == NULL) {
        sc_info("connect to %s:%u ...", saddr, notify->port);
        sc_net_connect(saddr, notify->port, false, s->serviceid, 0);
    } else {
        // todo address update
    }
}
void
node_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct UM_BASE* um = msg;
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
        sc_net_subscribe(nm->connid, true);
        break;
    case NETE_CONNECT:
        sc_info("connect to node ok, %d", nm->connid);
        sc_net_subscribe(nm->connid, true);
        _reg_request(nm->connid);
        break;
    case NETE_CONNERR:
        sc_error("connect to node fail: %s", sc_net_error(nm->error));
        break;
    case NETE_SOCKERR:
        sc_error("node disconnect: %s, %d", sc_net_error(nm->error), nm->connid);
        sc_node_disconnect(nm->connid);
        break;
    }
}
