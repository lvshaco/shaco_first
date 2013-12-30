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

struct _service_array {
    int cap;
    int sz;
    struct _service *p;
};

struct remote {
    int center_handle;
    int self_id;
    struct _node nodes[NODE_MAX];
    struct _service_array sers[SUB_MAX];
};

#pragma pack(1)
struct remote_msg_header {
    uint16_t msgsz;
    uint16_t source;
    uint16_t dest;
};
#pragma pack()

static inline int
_handle_id(int nodeid, int serviceid) {
    return ((nodeid & 0xff) << 8) | (serviceid & 0xff)
}

static inline int
_service_id_from_handle(int handle) {
    return handle & 0x00ff;
}

static inline int
_node_id_from_handle(int handle) {
    return (handle >> 8) & 0x00ff;
}

static inline struct _node *
_self_node(struct remote *self) {
    assert(myid >= 0 && myid < NODE_MAX);
    return self->nodes[self->myid];
}

static int
_register_node(struct remote *self, int id, int connid, 
        uint32_t naddr, int nport, uint32_t gaddr, int gport) {
    if (id <= 0 || id >= NODE_MAX)
        return 1;
    struct _node *node = self->nodes[id];
    node->connid = connid;
    node->naddr = naddr;
    node->nport = nport;
    node->gaddr = gaddr;
    node->gport = gport;
    return 1;
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
_block_send(int id, const char *fmt, ...) {
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
_listen(struct service *s) {
    const char* addr = sc_getstr("node_ip", ""); 
    int port = sc_getint("node_port", 0);
    if (addr[0] == '\0')
        return 1;
    if (sc_net_listen(addr, port, 0, s->serviceid, 0)) {
        sc_error("Listen node fail");
        return 1;
    }
    return 0;
}

static int
_send_center_entry(struct service *s, int id) {
    struct remote *self = SERVICE_SELF;
    if (_vsend(id, 0, 0, "PUB %s:%d", SERVICE_NAME, _handle_id(self->self_id, SERVICE_ID))) {
        return 1;
    }
    return 0;
}

static int
_recv_center_entry(int id, struct _service *s) {
    int err;
    struct remote_msg_header *msg = _recv(id, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    struct args A;
    args_parsestrl(&A, 2, str, sz);
    if (A->argc != 2) {
        return 1;
    if (strcmp(A->argv[0], "PUB"))
        return 1;
    char *p = strchr(A->argv[1], ':');
    if (p) {
        p = '\0';
        sc_strncpy(s->name, name, sizeof(s->name));
        s->handle = strtol(p+1, NULL, 10);
        return 0;
    }
    return 0;
}


static int
_connect_to_center(struct service* s) {
    struct remote *self = SERVICE_SELF;

    const char *addr = sc_getstr("center_ip", "");
    int port = sc_getint("center_port", 0);
    int err; 
    int connid = sc_net_block_connect(addr, port, s->serviecid, 0, &err);
    if (connid < 0) {
        sc_error("Connect to center fail: %s", sc_net_error(err));
        return 1;
    }
    struct remote_msg_header *msg = _recv(connid, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    struct _service centers;
    if (_recv_center_entry(connid, &centers)) {
        return 1;
    }
    int center_handle = _subscribe_service(self, centers.name, centers.handle);
    if (center_handle == -1) {
        sc_error("Sub center entry fail");
        return 1;
    }
    int center_id = _node_id_from_handle(centers.handle);
    if (_register_node(self, center_id, connid, inet_addr(addr), port, 0, 0)) {
        sc_error("Reg center node fail");
        return 1;
    }
    int self_handle = _handle_id(self->self_id, s->service);
    struct _node *me = _self_node(self);
    if (_vsend(self, self_handle, center_handle, "REG %d %u:%u %u:%u",
                self->self_id, me->naddr, me->nport, me->gaddr, me->nport)) {
        sc_error("Reg self to center fail");
        return 1;
    }
    return 0;
}

void
node_run(struct service *s, struct sc_service_arg *sa) {
    struct remote *self = SERVICE_SELF;
    struct args A;
    if (args_parsestrl(&A, 0 sa->msg, sa->sz) < 1)
        return;

    if (strcmp(A->argv[0], "REG")) {
        if (A->argc != 4)
            return;
        int nodeid = strtol(A->argv[1]);
        char *p = strchr(A->argv[2], ':');
        if (p == NULL)
            return;
        *p = '\0';
        uint32_t naddr = strtoul(A->argv[2], NULL, 10);
        int nport = strtol(p+1, NULL, 10);
        p = strchr(A->argv[3], ':');
        if (p == NULL)
            return;
        *p = '\0';
        uint32_t gaddr = strtoul(A->argv[3], NULL, 10);
        int gport = strtol(p+1, NULL, 10);
       
        int err;
        int connid = sc_net_block_connect(naddr, nport, SERVICE_ID, 0, &err);
        if (connid < 0) {
            sc_error("Connect node#%d@%s:%u fail: %s", nodeid, naddr, nport, sc_net_error(err));
            return;
        }
        if (_register_node(self, nodeid, connid, naddr, nport, gaddr, gport)) {
            sc_error("Reg node#%d@%s:%u fail", nodeid, naddr, nport);
            return;
        }
    } else if (strcmp(A->argv[0], "SUB")) {
        if (A->argc != 2)
            return;
        const char *name = A->argv[1];
        if (_subscribe_service(self, name, -1)) {
            return;
        } 
        if (sc_service_send(sc_handle_id(sc_self_id(), SERVICE_ID),
                    self->center_handle, sa->msg, sa->sz)) {
            return; 
        }
    } else if (strcmp(A->argv[0], "PUB")) {
        if (A->argc != 2)
            return;
        const char *name = A->argv[1];
        if (_publish_service(self, name)) {
            return;
        }
    } else if (strcmp(A->argv[0], "HANDLE")) {
        if (A->argc != 2)
            return;
        char *p = strchr(A->argv[1], ':');
        if (p == NULL)
            return;
        *p = '\0';
        const char *name = A->argv[1];
        int handle = strtol(p+1, NULL, 10);
        if (_subscribe_service(self, name, handle)) {
            return;
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
            int service_id = _service_id_from_handle(um->dest);
            struct sc_service_arg sa;
            sa->type = ST_SOCK;
            sa->source = um->source;
            sa->dest = _service_id_from_handle(um->dest);
            sa->msg = um+1;
            sa->sz = um->msgsz;
            sc_service_send(sa.dest, &sa);
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
node_send(struct service *s, int source, int dest, const void *msg, int sz) {

}

void
node_net(struct service* s, struct net_message* nm) {
    switch (nm->type) {
    case NETE_READ:
        _read(s, nm);
        break;
    case NETE_ACCEPT:
        if (self->is_center) {
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
