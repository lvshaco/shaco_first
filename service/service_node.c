#include "sc_service.h"
#include "sc.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_node.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <assert.h>

#define NODE_MAX 256
#define SUB_MAX 16
#define MSG_MAX 60*1024

struct _node {
    int connid;
    uint32_t naddr;
    uint16_t nport;
    uint32_t gaddr;
    uint16_t gport;
    int node_handle;
};

struct _service {
    char name[32];
    int handle;
};

struct _service_array {
    int cap;
    int sz;
    struct _service *p;
};

struct remote {
    int iscenter;
    int center_handle;
    int myid;
    struct _node nodes[NODE_MAX];
    struct _service_array sers;
};

#pragma pack(1)
struct remote_msg_header {
    uint16_t sz;
    uint16_t source;
    uint16_t dest;
};
#pragma pack()

// node 
static inline struct _node *
_mynode(struct remote *self) {
    assert(self->myid > 0 && self->myid < NODE_MAX);
    return &self->nodes[self->myid];
}

static int
_disconnect_node(struct remote *self, int connid) {
    int i;
    for (i=0; i<NODE_MAX; ++i) {
        if (self->nodes[i].connid == connid) {
            self->nodes[i].connid = -1;
            return 0;
        }
    }
    return 1;
}

// net
static struct remote_msg_header *
_recv(int id, int *err) {
    int nread;
    struct mread_buffer buf;
    struct remote_msg_header *msg;
    for (;;) {
        nread = sc_net_read(id, false, &buf, err);
        if (*err == 0) {
            if (buf.sz >= sizeof(*msg)) {
                msg = buf.ptr;
                if (buf.sz >= msg->sz) {
                    buf.ptr += msg->sz;
                    buf.sz -= msg->sz;
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
_send(struct remote *self, int source, int dest, void *msg, size_t sz) {
    int nodeid = (dest & 0xff00) >> 8;
    assert(nodeid > 0 && nodeid < NODE_MAX);
    struct _node* node = &self->nodes[nodeid];
    if (node->connid != -1) {
        char tmp[MSG_MAX];
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
    char msg[MSG_MAX];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    return _send(self, source, dest, msg, n);
}

// service
static int
_subscribe_service(struct remote *self, const char *name, int handle) {
    struct _service_array *sers = &self->sers;
    if (name[0] == '\0') {
        sc_error("Subscribe null service");
        return -1;
    }
    int i;
    for (i=0; i<sers->sz; ++i) {
        if (strcmp(sers->p[i].name, name)) {
            sers->p[i].handle = handle;
            return 0x8000 | i;
        }
    }
    if (sers->sz >= SUB_MAX) {
        sc_error("Subscribe too much service");
        return -1;
    }
    if (sers->sz <= sers->cap) {
        sers->cap *= 2;
        if (sers->cap == 0)
            sers->cap = 1;
        sers->p = realloc(sers->p, sizeof(struct _service) * sers->cap);
        memset(sers->p+sers->sz, 0, sers->cap - sers->sz);
    }
    int id = sers->sz++;
    struct _service *s = &sers->p[id];
    sc_strncpy(s->name, name, sizeof(s->name));
    s->handle = handle;
    return 0x8000 | id;
}

static int
_publish_service(struct remote *self, const char *name) {
    int handle = sc_service_query_id(name);
    if (handle == -1) {
        return 1;
    }
    handle &= 0xff;
    handle |= (nodeid << 8) & 0xff00;
    return _vsend(self, 0, handle, "PUB %s:%d", name, handle);
}

// initialize
static int
_init_mynode(struct remote *self) {
    int id = sc_getint("node_id", 0);
    if (id <= 0 || id >= NODE_MAX) {
        sc_error("Node id must be 1 ~ %d", NODE_MAX);
        return 1;
    }
    struct _node *node = &self->nodes[id];
    node->connid = -1;
    node->naddr = inet_addr(sc_getstr("node_ip", "0"));
    node->nport = sc_getint("node_port", 0);
    node->gaddr = inet_addr(sc_getstr("gate_ip", "0"));
    node->gport = sc_getint("gate_port", 0); 
    return 0;
}

static int
_listen(struct service *s) {
    const char *addr = sc_getstr("node_ip", "0");
    int port = sc_getint("node_port", 0);
    int err;
    int id = sc_net_listen(addr, port, 0, s->serviceid, 0, &err);
    if (id < 0) {
        sc_error("Listen %s:%d fail: %s", addr, port, sc_net_error(err));
        return 1;
    }
    return 0;
}

static bool
_iscenter() {
    const char *addr = sc_getstr("center_ip", "");
    int port = sc_getint("center_port", 0);
    return addr[0] != '\0' && port != 0;
}

static int
_send_center_entry(struct service *s, int id) {
    struct remote *self = SERVICE_SELF;
    if (_vsend(id, 0, 0, "PUB %s:%d", SERVICE_NAME, sc_handleid(self->myid, SERVICE_ID))) {
        return 1;
    }
    return 0;
}

static int
_recv_center_entry(int id, int *center_handle, int *node_handle) {
    int err;
    struct remote_msg_header *msg = _recv(id, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    struct args A;
    args_parsestrl(&A, 2, str, sz);
    if ((A.argc == 2) &&
        (!strcmp(a->argv[0], "PUB")) &&
        (p = strchr(A.argv[1], ':'))) {
        p = '\0';
        sc_strncpy(s->name, name, sizeof(s->name));
        s->handle = strtol(p+1, NULL, 10);
        return 0;
    } else {
        sc_error("Recv invaild center entry");
        return 1;
    }
}

static int
_update_node(struct remote *self, int nodeid, const char *naddr, const char *gaddr, 
        int entry, int connid) {
    if (nodeid <= 0 || nodeid >= NODE_MAX) {
        return 1;
    }
    struct _node *node = &self->nodes[nodeid];
    char *p;
    p = strchr(naddr, ':');
    if (p) {
        node->naddr = inet_addr(naddr);
        node->nport = strtoul(p+1, NULL, 10);
    } else {
        return 1;
    } 
    p = strchr(gaddr, ':');
    if (p) {
        node->gaddr = inet_addr(gaddr);
        node->gport = strtoul(p+1, NULL, 10);
    }
    if (entry != -1) {
        node->node_handle = entry;
    }
    if (connid != -1) {
        node->connid = connid;
    }
    return 0;
}

static int
_connect_node(struct remote *self, int nodeid, int serviceid) {
    if (nodeid <= 0 || nodeid >= NODE_MAX) {
        return 1;
    }
    struct _node *node = &self->nodes[nodeid];
    int err;
    int connid = sc_net_block_connect(node->naddr, node->nport, serviceid, 0, &err);
    if (connid < 0) {
        sc_error("Connect node#%d@%s:%u fail: %s", 
                nodeid, node->naddr, node->nport, sc_net_error(err));
        return 1;
    }
    node->connid = connid;
    return 0;
}

static int
_broadcast_node(struct service *s, int nodeid) {
    struct remote *self = SERVICE_SELF;

    if (nodeid <= 0 || nodeid >= NODE_MAX) {
        return 1;
    }
    struct _node *node = &self->nodes[nodeid];
    if (node->connid == -1) {
        return 1;
    }

    int i;
    struct _node *other;
    
    // boradcast me
    for (i=0; i<NODE_MAX; ++i) {
        other = &self->nodes[i];
        if (i == nodeid || i == self->myid) 
            continue;
        if (other->connid == -1) 
            continue;
        sc_service_send(SERVICE_ID, other->node_handle, "ADDR %d %u:%u %u:%u",
                self->myid, me->naddr, me->nport, me->gaddr, me->gport);
    }

    // get other
    for (i=0; i<NODE_MAX; ++i) {
        other = &self->nodes[i];
        if (i == nodeid)
            continue;
        if (other->connid == -1)
            continue;
        sc_service_send(SERVICE_ID, node->node_handle, "ADDR %d %u:%u %u:%u",
                i, other->naddr, other->nport, other->gaddr, other->gport);
    }
}

static int
_connect_to_center(struct service* s) {
    struct remote *self = SERVICE_SELF;

    const char *addr = sc_getstr("center_ip", "0");
    int port = sc_getint("center_port", 0);
    int err; 
    int connid = sc_net_block_connect(addr, port, s->serviecid, 0, &err);
    if (connid < 0) {
        sc_error("Connect to center fail: %s", sc_net_error(err));
        return 1;
    }
    int center_handle, node_handle;
    if (_recv_center_entry(connid, &center_handle, &node_handle)) {
        return 1;
    }
    int center_id = sc_nodeid_from_handle(center_handle);
    if (_update_node(self, center_id, inet_addr(addr), port, node_handle, connid)) {
        sc_error("Reg center node fail");
        return 1;
    }
    int self_handle = sc_handleid(self->myid, s->service);
    struct _node *me = _mynode(self);
    if (_vsend(self, self_handle, center_handle, "REG %d %u:%u %u:%u %d",
                self->myid, me->naddr, me->nport, me->gaddr, me->gport, 
                self_handle)) {
        sc_error("Reg self to center fail");
        return 1;
    }
    return 0;
}

struct remote *
node_create() {
    struct remote* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    self->center_handle = -1;
    return self;
}

void
node_free(struct node* self) {
    if (self == NULL)
        return;
    free(self->sers.p);
    free(self);
}

int
node_init(struct service* s) {
    struct remote *self = SERVICE_SELF;
    if (_init_mynode(self)) {
        return 1;
    }
    if (_listen(s)) {
        return 1;
    }
    self->iscenter = _iscenter();
    if (!self->iscenter) {
        _connect_to_center(s);
    }
    return 0;
}

static inline struct remote_msg_header * 
_read_one(struct mread_buffer *buf, int *err) {
    *err = 0;
    struct remote_msg_header *base = buf->ptr;
    int sz = buf->sz;
    int body;
    if (sz >= sizeof(*base)) {
        sz -= sizeof(*base);
        if (base->msgsz >= sizeof(*base)) {
            body = base->msgsz - sizeof(*base);
            if (body > 0) {
                if (body <= sz) {
                    goto ok;
                } else {
                    return NULL;
                }
            } else if (body < 0) {
                return NULL;
            } else {
                goto ok;
            }
        } else {
            *err = NET_ERR_MSG;
            return NULL;
        }
    } else {
        return NULL;
    }
ok:
    buf->ptr += base->msgsz;
    buf->sz  -= base->msgsz;
    return base;
}

static void
_read(struct service *s, struct net_message *nm) {
    assert(nm->type == NETE_READ);
    struct dispatcher* self = SERVICE_SELF;
    int id = nm->connid; 
    int step = 0; 
    int drop = 1;     
    for (;;) {
        int error = 0;
        struct mread_buffer buf;
        int nread = sc_net_read(id, drop==0, &buf, &error);
        if (nread <= 0) {
            mread_throwerr(nm, error);
            return;
        }
        struct remote_msg_header* um;
        while ((um = _read_one(&buf, &error))) {
            sc_service_send(um->source, sc_serviceid_from_handle(um->dest), um+1, um->msgsz);
            if (++step > 1000) {
                sc_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (error) {
            sc_net_close_socket(id, true);
            mread_throwerr(nm, error);
            return;
        }
        drop = nread-buf.sz;
        sc_net_dropread(id, drop);       
    }
}

void
node_send(struct service *s, int session, int dest, const void *msg, int sz) {
    // todo
}

void
node_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    case NETE_READ:
        _read(s, nm);
        break;
    case NETE_ACCEPT:
        if (self->iscenter) {
            _send_center_entry(s, nm->connid);
        }
        sc_net_subscribe(nm->connid, true);
        break;
    // todo do not do this
    //case NETE_CONNECT:
        //sc_info("connect to node ok, %d", nm->connid);
        //break;
    //case NETE_CONNERR:
        //sc_error("connect to node fail: %s", sc_net_error(nm->error));
        //break;
    case NETE_SOCKERR:
        sc_error("node disconnect: %s, %d", sc_net_error(nm->error), nm->connid);
        _disconnect_node(self, nm->connid);
        break;
    }
}

void
node_main(struct service *s, int session, int source, const void *msg, int sz) {
    struct remote *self = SERVICE_SELF;
    struct args A;
    if (args_parsestrl(&A, 0, msg, sz) < 1)
        return;

    const char *cmd = A.argv[0];
    if (strcmp(cmd, "REG")) {
        if (A.argc != 5)
            return;
        int nodeid = strtol(A.argv[1], NULL, 10);
        const char *naddr = A.argv[2];
        const char *gaddr = A.argv[3];
        int node_handle = strtol(A.argv[4], NULL, 10);
        if (!_update_node(self, nodeid, naddr, gaddr, handle, -1)) {
            _connect_node(self, nodeid, SERVICE_ID);
        }
    } else if (strcmp(cmd, "ADDR")) {
        if (A.argc != 5)
            return;
        int nodeid = strtol(A.argv[1], NULL, 10);
        const char *naddr = A.argv[2];
        const char *gaddr = A.argv[3];
        _update_node(self, nodeid, naddr, gaddr, -1, -1);
    } else if (strcmp(cmd, "BROADCAST")) {
        if (A.argc != 2)
            return;
        int nodeid = strtol(A.argv[1], NULL, 10);
        _broadcast_node(s, nodeid);
    } else if (strcmp(cmd, "SUB")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        if (!_subscribe_service(self, name, -1)) {
            sc_service_send(SERVICE_ID, self->center_handle, msg, sz);
        }
    } else if (strcmp(cmd, "PUB")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        _publish_service(self, name);
    } else if (strcmp(cmd, "HANDLE")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        char *p = strchr(name, ':');
        if (p) {
            *p = '\0';
            int handle = strtol(p+1, NULL, 10);
            _subscribe_service(self, name, handle);
        }
    }
}
