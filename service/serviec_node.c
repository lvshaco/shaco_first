#include "host_service.h"
#include "node_type.h"
#include <stdlib.h>
#include <arpa/inet.h>

static void
_mynode(struct host_node* node) {
    node->tid  = host_node_typeid(host_getstr("node_type", ""));
    node->sid  = host_getint("node_sid", 0);
    node->addr = inet_addr(host_getstr("node_ip", ""));
    node->port = host_getint("node_port", 0);
    node->connid = -1;
}

static bool
_iscenter(int id) {
    uint32_t addr;
    int port;
    if (host_net_socket_address(id, &addr, &port) == 0 &&
        addr == inet_addr(host_getstr("center_ip", "")) &&
        port == host_getint("center_port", 0)) {
        return true;
    }
    return false;
}

static int
_connect_center() {
    const char* addr = host_getstr("center_ip", "");
    int port = host_getint("center_port", 0);
    if (host_net_connect(addr, port, true, s->serviceid) < 0) {
        host_error("connect to center fail");
        return 1;
    }
    return 0;
}

static int
_listen() {
    const char* addr = host_getstr("node_ip", "");
    int port = host_getint("node_port", 0);
    if (addr[0] != '\0' &&
        host_net_listen(addr, port, s->serviceid)) {
        host_error("listen node fail");
        return 1;
    }
    return 0;
}

int
node_init(struct service* s) {
    if (_connect_center())
        return 1;
    if (_listen())
        return 1;
    host_node_register_types(NODE_NAMES, NODE_MAX);
    SUBSCRIBE_MSG(s->serviceid, UMID_NODE_REG);
    return 0;
}

static void
_reg_request(int connid) {
    struct host_node my;
    _mynode(&my);
    UM_DEFFIX(UM_node_reg, reg, UMID_NODE_REG);
    reg.tid = my.tid;
    reg.sid = my.sid;
    reg.addr = my.addr;
    reg.port = my.port;
    UM_SEND(connid, &reg, sizeof(reg));
}

static void
_reg(int id, struct UM_node_reg* reg) {
    struct host_node node;
    node.tid = reg->tid;
    node.sid = reg->sid;
    node.addr = reg->addr;
    node.port = reg->port;
    node.connid = id;
    if (host_node_register(&node)) {
        host_net_close_socket(id);
    }
}

void
node_usermsg(struct service* s, int id, void* msg, int sz) {
    struct user_message* um = msg;
    switch (um->msgid) {
    case UMID_NODE_REG:
        _reg(id, um);
        break;
    }
}

void
node_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    //case NETE_ACCEPT:
        //break;
    case NETE_CONNECT:
        _reg_request(nm->connid);
        break;
    case NETE_SOCKERR:
        host_node_disconnect(nm->connid);
        break;
    //case NETE_WRITEDONE:
        //break;
    }
}

//void
//node_time(struct service* s) {
//}
