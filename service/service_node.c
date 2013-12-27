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

#define NODE_MAX 256
#define SUB_MAX 10
#define NODE_MASK 0xff00

struct _node {
    int connid;
    uint32_t naddr;
    uint16_t nport;
    uint32_t gaddr;
    uint16_t gport;
};

struct _service {
    char name[32];
    int handle;
};

struct remote {
    int myid;
    struct _node nodes[NODE_MAX];
    struct _service sers[SUB_MAX];
};

struct remote_msg_header {
    uint16_t msgsz;
    uint16_t source;
    uint16_t dest;
};

static struct _node *
_me(struct remote *self) {
    assert(myid >= 0 && myid < NODE_MAX);
    return self->nodes[self->myid];
}

static int
_connect(struct service* s, const char* addr, int port) {
    int err;
    int id = sc_net_block_connect(addr, port, s->serviecid, 0, &err);
    if (id < 0) {
        sc_error(sc_net_error(err));
    } 
    return id;
}

static int
_send(int id, const char *fmt, ...) {
    char msg[UM_MAXSZ];
    int sz = sizeof(msg);
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg+2, sz-2, fmt, ap);    
    va_end(ap);

    if (n > UM_MAXSZ) {
        sc_panic("too max packet");
    }
    *(uint16_t*)msg = n; 
    int err;
    if (sc_net_block_send(id, msg, n+2, &err) != n+2) {
        sc_error(sc_net_error(err));
        return 1;
    }
    return 0;
}

static struct remote_msg_header *
_recv(int id, int *err) {
    int nread;
    struct mread_buffer buf;
    struct remote_msg_header *msg;
    for (;;) {
        nread = sc_net_read(id, false, &buf, err);
        if (*err == 0) {
            if (buf->sz >= sizeof(*msg)) {
                msg = buf->ptr;
                if (buf->sz >= msg->sz) {
                    buf->ptr += msg->sz;
                    buf->sz -= msg->sz;
                    sc_net_dropread(id, nread-buf.sz);
                    return msg;
                }
            }
        } else {
            return NULL;
        }
    }
}

static int
_dsend(int connid, int source, int dest, void *msg, size_t sz) {
    char tmp[UM_MAXSZ];
    int len = sz+6;
    if (len <= sizeof(tmp)) {
        uint16_t *p = tmp;
        *p++ = len;
        *p++ = source;
        *p++ = dest;
        memcpy(p, msg, sz);
        return sc_net_send(connid, tmp, len);
    } else {
        sc_error("too large msg from %d to %d", source, dest);
        return 1;
    }
}

static int
_vdsend(int connid, int source, int dest, const char *fmt, ...) {
    // todo fix it, with _send
    char msg[UM_MAXSZ];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    return _vsend(connid, source, dest, msg, n);
}

static int
_send(struct remote *self, int source, int dest, void *msg, size_t sz) {
    int nodeid = (dest & 0xff00) >> 8;
    assert(nodeid > 0 && nodeid < NODE_MAX);
    struct _node* node = &self->nodes[nodeid];
    if (node->connid != -1) {
        _dsend(node->connid, source, dest, msg, sz);
        return 0;
    } else {
        sc_error("node %d has not connect", nodeid);
        return 1;
    } 
}

static int
_vsend(struct remote *self, int source, int dest, const char *fmt, ...) {
    // todo fix it, with _send
    char msg[UM_MAXSZ];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    return _send(self, source, dest, msg, n);
}

static int
_subscribe(struct remote *self, int id, const char *name) {
    assert(id > 0 && id < SUB_MAX);
    if (name[0] == '\0')
        return -1;
    struct _service *s = &self->sers[id];
    if (s->name[0] == '\0') {
        sc_strncpy(s->name, name, sizeof(s->name));
    } else {
        if (strcmp(s->name, name)) {
            sc_error("Conflict service subscibe, %s -> [%d]%s", name, id, s->name);
            return -1;
        }
    }
    return 0x8000 | id;
}

static int
_publish(struct remote *self, const char *name) {
    int handle = sc_service_query_id(name);
    if (handle == -1) {
        return 1;
    }
    handle &= 0xff;
    handle |= (nodeid << 8) & 0xff00;
    return _vsend(self, 0, handle, "PUB %s:%d", name, handle);
}

static int
_init_mynode(struct remote *self) {
    int id = sc_getint("node_id", 0);
    if (id < 0 || id >= NODE_MAX)
        return 1;
    struct _node *node = &self->nodes[id];
    node->connid = -1;
    node->naddr = inet_addr(sc_getstr("node_ip", "0"));
    node->nport = sc_getint("node_port", 0);
    node->gaddr = inet_addr(sc_getstr("gate_ip", "0"));
    node->gport = sc_getint("gate_port", 0); 
    return 0;
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

static int
_connect_to_center(struct service* s) {
    struct remote *self = SERVICE_SELF;

    const char* addr = sc_getstr("center_ip", "");
    int port = sc_getint("center_port", 0);
    int err; 
    int id = sc_net_block_connect(addr, port, s->serviecid, 0, &err);
    if (id < 0) {
        sc_error("Connect to center fail: %s", sc_net_error(err));
        return 1;
    }
   
    struct remote_msg_header *msg = _recv(id, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    int center_handle = msg->source;
    int self_handle   = HANDLE_SELF;
    struct _node *me = _me(self);
    if (_vdsend(id, 0, self_handle, center_handle, "REG %d %u:%u %u:%u",
                self->myid, me->naddr, me->nport, me->gaddr, me->nport)) {
        sc_error("Reg to center fail");
        return 1;
    }
    return 0;
}

void
node_run(struct service *s, struct sc_service_arg *a) {
    struct remote *self = SERVICE_SELF;
    switch (a->type) {
    case ST_SOCK:
        _send(self, a->source, a->dest, a->msg, a->sz);
        break; 
    case ST_SERV: {
        struct args A;
        if (args_parsestrl(&A, 3, a->msg, a->sz) < 3)
            return;
        if (strcmp(A->argv[0], "SUB")) {
            int id = strtol(A->argv[1], NULL, 10);
            const char *name = A->argv[2];
            _subscribe(self, id, name);
        } else if (strcmp(A->argv[0], "PUB")) {
            const char *name = A->argv[1];
            _publish(self, name);
        }
        break;
        }
    }
}

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
