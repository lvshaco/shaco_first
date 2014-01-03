#include "sc_service.h"
#include "sc.h"
#include "sc_util.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_node.h"
#include "args.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#define NODE_MAX 256
#define MSG_MAX 60*1024

struct _node {
    int connid;
    char naddr[40]; // ipv6 support
    int nport;
    char gaddr[40]; // ipv6 support
    int gport;
    int node_handle;
};

struct remote {
    int center_handle;
    int myid;
    struct _node nodes[NODE_MAX];
};

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
static void *
_recv(int id, int *msgsz, int *err) {
    int nread;
    struct mread_buffer buf;
    void *msg;
    for (;;) {
        nread = sc_net_read(id, false, &buf, err);
        if (*err == 0) {
            if (buf.sz > 6) {
                msg = buf.ptr+6;
                int sz = *(uint16_t*)buf.ptr + 2;
                if (buf.sz >= sz) {
                    buf.ptr += sz;
                    buf.sz -= sz;
                    sc_net_dropread(id, nread-buf.sz);
                    *msgsz = sz-6;
                    return msg;
                }
            }
        } else {
            return NULL;
        }
    }
}
/*
static int
_block_send(struct remote *self, int id, int source, int dest, const void *msg, int sz) {
    source |= (self->myid << 8);
    dest &= 0xff;
    char head[6];
    int len = sizeof(head)+sz;
    head[0] = len & 0xff;
    head[1] = (len >> 8) & 0xff;
    head[2] = source & 0xff;
    head[3] = (source >> 8) & 0xff;
    head[4] = dest & 0xff;
    head[5] = (dest >> 8) & 0xff;
    if (sc_net_block_send(id, head, sizeof(head), &err) != sizeof(head)) {
        return 1;
    }
    if (sc_net_block_send(id, msg, sz, &err) != sz) {
        return 1;
    }
    return 0;
}
*/

static int
_dsend(struct remote *self, int connid, int source, int dest, const void *msg, size_t sz) {
    char tmp[MSG_MAX];
    int len = sz+6;
    if (len <= sizeof(tmp)) {
        source |= (self->myid << 8);
        dest &= 0xff;
        uint16_t *p = (uint16_t*)tmp;
        *p++ = len-2;
        *p++ = source;
        *p++ = dest;
        memcpy(p, msg, sz);
        return sc_net_send(connid, tmp, len);
    } else {
        sc_error("Too large msg from %0x to %0x", source, dest);
        return 1;
    }
}

static int
_vdsend(struct remote *self, int connid, int source, int dest, const char *fmt, ...) {
    char msg[MSG_MAX];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sc_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    return _dsend(self, connid, source, dest, msg, n);
}


static int
_send(struct remote *self, int source, int dest, const void *msg, size_t sz) {
    int nodeid = sc_nodeid_from_handle(dest);
    if (nodeid <= 0 || nodeid >= NODE_MAX) {
        sc_error("Invalid nodeid from dest %0x", dest);
        return 1;
    }
    struct _node* node = &self->nodes[nodeid];
    if (node->connid != -1) {
        return _dsend(self, node->connid, source, dest, msg, sz);
    } else {
        sc_error("Node %d has not connect", nodeid);
        return 1;
    } 
}

static int
_vsend(struct remote *self, int source, int dest, const char *fmt, ...) {
    char msg[MSG_MAX];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sc_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    return _send(self, source, dest, msg, n);
}

// service
static int
_publish_service(struct remote *self, const char *name, int handle) {
    handle &= 0xff;
    handle |= (self->myid << 8) & 0xff00;
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
    self->myid = id;
    struct _node *node = &self->nodes[id];
    node->connid = -1;
    sc_strncpy(node->naddr, sc_getstr("node_ip", "0"), sizeof(node->naddr));
    node->nport = sc_getint("node_port", 0);
    sc_strncpy(node->gaddr, sc_getstr("gate_ip", "0"), sizeof(node->gaddr));
    node->gport = sc_getint("gate_port", 0); 
    return 0;
}

static int
_listen(struct service *s) {
    const char *addr = sc_getstr("node_ip", "0");
    int port = sc_getint("node_port", 0);
    if (sc_net_listen(addr, port, 0, s->serviceid, 0)) {
        return 1;
    }
}

static int
_send_center_entry(struct service *s, int id) {
    struct remote *self = SERVICE_SELF;
    if (_vdsend(self, id, 0, 0, "%d %d", 
                sc_handleid(self->myid, self->center_handle),
                sc_handleid(self->myid, SERVICE_ID))) {
        return 1;
    }
    return 0;
}

static int
_recv_center_entry(int id, int *center_handle, int *node_handle) {
    int err;
    int msgsz;
    void *msg = _recv(id, &msgsz, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    struct args A;
    args_parsestrl(&A, 2, msg, msgsz);
    if (A.argc == 2) {
        *center_handle = strtol(A.argv[0], NULL, 10);
        *node_handle = strtol(A.argv[1], NULL, 10);
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
        *p = '\0';
        sc_strncpy(node->naddr, naddr, sizeof(node->naddr));
        node->nport = strtoul(p+1, NULL, 10);
    } else {
        return 1;
    } 
    p = strchr(gaddr, ':');
    if (p) {
        *p = '\0';
        sc_strncpy(node->gaddr, gaddr, sizeof(node->gaddr));
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
_connect_node(struct service *s, int nodeid) {
    struct remote *self = SERVICE_SELF;
    if (nodeid <= 0 || nodeid >= NODE_MAX) {
        sc_error("Connect invalid node %d", nodeid);
        return 1;
    }
    struct _node *node = &self->nodes[nodeid];
    int err;
    int connid = sc_net_block_connect(node->naddr, node->nport, SERVICE_ID, 0, &err);
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
        _vsend(self, SERVICE_ID, other->node_handle, "ADDR %d %u:%u %u:%u",
                self->myid, node->naddr, node->nport, node->gaddr, node->gport);
    }

    // get other
    for (i=0; i<NODE_MAX; ++i) {
        other = &self->nodes[i];
        if (i == nodeid)
            continue;
        if (other->connid == -1)
            continue;
        _vsend(self, SERVICE_ID, node->node_handle, "ADDR %d %u:%u %u:%u",
                i, other->naddr, other->nport, other->gaddr, other->gport);
    }
    return 0;
}

static int
_connect_to_center(struct service* s) {
    struct remote *self = SERVICE_SELF;

    const char *addr = sc_getstr("center_ip", "0");
    int port = sc_getint("center_port", 0);
    int err; 
    int connid = sc_net_block_connect(addr, port, SERVICE_ID, 0, &err);
    if (connid < 0) {
        sc_error("Connect to center fail: %s", sc_net_error(err));
        return 1;
    }
    int center_handle, node_handle;
    if (_recv_center_entry(connid, &center_handle, &node_handle)) {
        return 1;
    }
    self->center_handle = center_handle;
    int center_id = sc_nodeid_from_handle(center_handle);
    int l = strlen(addr);
    char tmp[l+7];
    memcpy(tmp, addr, l);
    tmp[l] = ':';
    snprintf(tmp+l+1, 6, "%u", port); 
    if (_update_node(self, center_id, tmp, "", node_handle, connid)) {
        sc_error("Reg center node fail");
        return 1;
    }
    int self_handle = sc_handleid(self->myid, SERVICE_ID);
    struct _node *me = _mynode(self);
    if (_vsend(self, self_handle, center_handle, "REG %d %s:%u %s:%u %d",
                self->myid, me->naddr, me->nport, me->gaddr, me->gport, 
                self_handle)) {
        sc_error("Reg self to center fail");
        return 1;
    }
    sc_info("Connect to center ok");
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
node_free(struct remote* self) {
    if (self == NULL)
        return;
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
    self->center_handle = service_query_id("centers");
    if (self->center_handle == -1) {
        if (_connect_to_center(s)) {
            return 1;
        }
    } 
    return 0;
}

static void
_read(struct service *s, struct net_message *nm) {
    int id = nm->connid; 
    int step = 0; 
    int drop = 1;     
    int err;
    for (;;) {
        struct mread_buffer buf;
        err = 0;
        int nread = sc_net_read(id, drop==0, &buf, &err);
        if (nread <= 0) {
            if (!err)
                return;
            else
                goto errout;
        }
        for (;;) {
            if (buf.sz < 6) {
                break;
            }
            uint16_t *p = buf.ptr;
            uint16_t msgsz = *(p++) + 2;
            if (msgsz <= 6 || msgsz > MSG_MAX) {
                err = NET_ERR_MSG;
                break;
            }
            if (buf.sz < msgsz) {
                break;
            }
            buf.ptr += msgsz;
            buf.sz  -= msgsz;
            uint16_t source = *p++;
            uint16_t dest = *p++;
            sc_service_send(source, sc_serviceid_from_handle(dest), p, msgsz-6);

            if (++step > 1000) {
                sc_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (err) {
            sc_net_close_socket(id, true);
            goto errout;
        }
        drop = nread-buf.sz;
        sc_net_dropread(id, drop);       
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    service_net(nm->ud, nm);
}

void
node_send(struct service *s, int session, int source, int dest, const void *msg, int sz) {
    struct remote *self = SERVICE_SELF;
    _send(self, source, dest, msg, sz);
}

void
node_net(struct service* s, struct net_message* nm) {
    struct remote *self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(s, nm);
        break;
    case NETE_ACCEPT:
        if (sc_nodeid_from_handle(self->center_handle) == 0) {
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
    char tmp[sz+1];
    memcpy(tmp, msg, sz);
    tmp[sz]='\0';
    sc_debug(tmp);
    
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
        if (!_update_node(self, nodeid, naddr, gaddr, node_handle, -1)) {
            _connect_node(s, nodeid);
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
        sc_service_send(SERVICE_ID, self->center_handle, msg, sz);
    } else if (strcmp(cmd, "PUB")) {
        if (A.argc != 3)
            return;
        const char *name = A.argv[1];
        int handle = strtol(A.argv[2], NULL, 10);
        _publish_service(self, name, handle);
    } else if (strcmp(cmd, "HANDLE")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        char *p = strchr(name, ':');
        if (p) {
            *p = '\0';
            int handle = strtol(p+1, NULL, 10);
            sc_service_bind(name, handle);
            int nodeid = sc_nodeid_from_handle(handle);
            _connect_node(s, nodeid);
        }
    }
}
