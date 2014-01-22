#include "sc_service.h"
#include "sc.h"
#include "sh_util.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_node.h"
#include "args.h"
#include "message.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#define NODE_MAX 256

struct _int_vector {
    int cap;
    int sz;
    int *pi;
};

struct _node {
    int connid;
    int node_handle;
    struct sh_node_addr addr; 
    struct _int_vector handles;
};

struct remote {
    int center_handle;
    int myid;
    struct _node nodes[NODE_MAX];
};

// node 
static inline void
_update_node(struct _node *no, const char *naddr, int nport, const char *gaddr, int gport) {
    sc_strncpy(no->addr.naddr, naddr, sizeof(no->addr.naddr));
    sc_strncpy(no->addr.gaddr, gaddr, sizeof(no->addr.gaddr));
    no->addr.nport = nport;
    no->addr.gport = gport;
}

static inline void
_bound_node_entry(struct _node *no, int handle) {
    no->node_handle = handle;
}

static inline void
_bound_node_connection(struct _node *no, int connid) {
    no->connid = connid;
}

static void
_bound_handle_to_node(struct _node *no, int handle) {
    struct _int_vector *handles = &no->handles;
    int i;
    for (i=0; i<handles->sz; ++i) {
        if (handles->pi[i] == handle) {
            return;
        }
    }
    if (handles->sz >= handles->cap) {
        handles->cap *= 2;
        if (handles->cap == 0)
            handles->cap = 1;
        handles->pi = realloc(handles->pi, sizeof(handles->pi[0]) * handles->cap);
    }
    handles->pi[handles->sz++] = handle;
}

static inline struct _node *
_get_node(struct remote *self, int id) {
    return (id>0 && id<NODE_MAX) ? &self->nodes[id] : NULL;
}

static inline struct _node *
_my_node(struct remote *self) {
    return _get_node(self, self->myid);
}

static void
_disconnect_node(struct remote *self, int connid) {
    struct _node *no = NULL;
    int i;
    for (i=0; i<NODE_MAX; ++i) {
        if (self->nodes[i].connid == connid) {
            no = &self->nodes[i];
            break;
        }
    }
    if (no) {
        sc_info("Node(%d) disconnect", (int)(no-self->nodes));
        for (i=0; i<no->handles.sz; ++i) {
            sc_service_exit(no->handles.pi[i]);
        }
        no->handles.sz = 0;
    }
}

// net
static void *
_block_read(int id, int *msgsz, int *err) {
    for (;;) {
        struct mread_buffer buf;
        int nread = sc_net_read(id, false, &buf, err);
        if (*err == 0) {
            if (buf.sz > 6) {
                void *msg = buf.ptr+6;
                int sz = sh_from_littleendian16((uint8_t*)buf.ptr) + 2;
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

static int
_dsend(struct remote *self, int connid, int source, int dest, int type, const void *msg, int sz) {
    if (sz <= UM_MAXSZ) {
        /*if (type == MT_TEXT) {
            char tmp[sz+1];
            memcpy(tmp, msg, sz);
            tmp[sz] = '\0';
            sc_debug("send %s", tmp);
        }
       */ 
        int len = sz+6;
        source &= 0x00ff;
        source |= (self->myid << 8);
        dest   &= 0x00ff;
        dest   |= (type << 8);
        //sc_debug("send connid %d, source %04x, dest %04x, type %d, sz %d", connid, source, dest, type, sz);
        uint8_t *tmp = malloc(len);
        sh_to_littleendian16(len-2, tmp);
        sh_to_littleendian16(source, tmp+2);
        sh_to_littleendian16(dest, tmp+4);
        memcpy(tmp+6, msg, sz);
        return sc_net_send(connid, tmp, len);
    } else {
        sc_error("Too large msg from %0x to %0x", source, dest);
        return 1;
    }
}

static int
_vdsend(struct remote *self, int connid, int source, int dest, const char *fmt, ...) {
    char msg[UM_MAXSZ];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sc_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    return _dsend(self, connid, source, dest, MT_TEXT, msg, n);
}


static int
_send(struct remote *self, int source, int dest, int type, const void *msg, size_t sz) {
    int id = sc_nodeid_from_handle(dest);
    struct _node *no = _get_node(self, id);
    if (no == NULL) {
        sc_error("Invalid nodeid from dest %04x", dest);
        return 1;
    }
    if (no->connid != -1) {
        return _dsend(self, no->connid, source, dest, type, msg, sz);
    } else {
        sc_error("Node %d has not connect, by dest %04x", id, dest);
        return 1;
    } 
}

static int
_vsend(struct remote *self, int source, int dest, const char *fmt, ...) {
    char msg[UM_MAXSZ];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n >= sizeof(msg)) {
        sc_error("Too large msg %s from %0x to %0x", fmt, source, dest);
        return 1;
    }
    return _send(self, source, dest, MT_TEXT, msg, n);
}

// service
static inline int
_publish_service(struct service *s, const char *name, int handle) {
    struct remote *self = SERVICE_SELF;
    handle &= 0xff;
    handle |= (self->myid << 8) & 0xff00;
    return sh_service_vsend(SERVICE_ID, self->center_handle, "PUB %s:%04x", name, handle);
}

// initialize
static int
_init_mynode(struct remote *self) {
    self->myid = sc_getint("node_id", 0);
    struct _node *no = _my_node(self);
    if (no == NULL) {
        sc_error("Invalid node_id %d", self->myid);
        return 1;
    }
    _update_node(no, 
            sc_getstr("node_ip", "0"), 
            sc_getint("node_port", 0),
            sc_getstr("gate_ip", "0"), 
            sc_getint("gate_port", 0));
    return 0;
}

static int
_listen(struct service *s) {
    struct remote *self = SERVICE_SELF;
    struct _node *my = _my_node(self);
    if (my == NULL ||
        sc_net_listen(my->addr.naddr, my->addr.nport, 0, s->serviceid, 0)) {
        return 1;
    }
    return 0;
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
_block_read_center_entry(int id, int *center_handle, int *node_handle) {
    int err;
    int sz;
    void *msg = _block_read(id, &sz, &err);
    if (msg == NULL) {
        sc_error("Recv center entry fail: %s", sc_net_error(err));
        return 1;
    }
    struct args A;
    args_parsestrl(&A, 2, msg, sz);
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
_connect_node(struct service *s, struct _node *no) {
    struct remote *self = SERVICE_SELF;
    if (no->connid != -1) {
        return 0;
    }
    struct sh_node_addr *addr = &no->addr;
    int id = no - self->nodes;
    int err; 
    int connid = sc_net_block_connect(addr->naddr, addr->nport, SERVICE_ID, 0, &err);
    if (connid < 0) {
        sc_error("Connect node(%d) %s:%u fail: %s", 
                id, addr->naddr, addr->nport, sc_net_error(err));
        return 1;
    }
    _bound_node_connection(no, connid);
    sc_info("Connect to node(%d) %s:%u ok", id, addr->naddr, addr->nport);
    return 0;
}

static int
_connect_service(struct service *s, const char *name, int handle) {
    struct remote *self = SERVICE_SELF;

    int id = sc_nodeid_from_handle(handle);
    struct _node *no = _get_node(self, id);
    if (no == NULL) {
        sc_error("Invalid node %d", id);
        return 1;
    }
    _connect_node(s, no);
    _bound_handle_to_node(no, handle);
    sc_service_start(name, handle, &no->addr);
    return 0;
}

static int
_broadcast_node(struct service *s, int id) {
    struct remote *self = SERVICE_SELF;
    struct _node *no = _get_node(self, id);
    if (no == NULL) {
        return 1;
    }
    if (no->connid == -1) {
        return 1;
    }
    int i;
    // boradcast me
    for (i=0; i<NODE_MAX; ++i) {
        struct _node *other = &self->nodes[i];
        if (i == id || i == self->myid) 
            continue;
        if (other->connid == -1) 
            continue;
        _vsend(self, SERVICE_ID, other->node_handle, "ADDR %d %s %u %s %u",
                id, no->addr.naddr, no->addr.nport, no->addr.gaddr, no->addr.gport);
    }

    // get other
    for (i=0; i<NODE_MAX; ++i) {
        struct _node *other = &self->nodes[i];
        if (i == id)
            continue;
        if (other->connid == -1)
            continue;
        _vsend(self, SERVICE_ID, no->node_handle, "ADDR %d %s %u %s %u",
                i, other->addr.naddr, other->addr.nport, other->addr.gaddr, other->addr.gport);
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
    if (_block_read_center_entry(connid, &center_handle, &node_handle)) {
        return 1;
    }
    int center_id = sc_nodeid_from_handle(center_handle);
    struct _node *no = _get_node(self, center_id);
    if (no == NULL) {
        sc_error("Reg center node fail");
        return 1;
    }
    _update_node(no, addr, port, "", 0);
    _bound_node_connection(no, connid);
    _bound_node_entry(no, node_handle);
    self->center_handle = center_handle;

    int self_handle = sc_handleid(self->myid, SERVICE_ID);
    struct _node *me = _my_node(self);
    if (_vsend(self, self_handle, center_handle, "REG %d %s %u %s %u %d",
                self->myid, me->addr.naddr, me->addr.nport, me->addr.gaddr, me->addr.gport, 
                self_handle)) {
        sc_error("Reg self to center fail");
        return 1;
    }
    sc_info("Connect to center(%d) %s:%u ok", center_id, addr, port);
    return 0;
}

struct remote *
node_create() {
    struct remote* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    self->center_handle = -1;
    int i;
    for (i=0; i<NODE_MAX; ++i) {
        self->nodes[i].connid = -1;
        self->nodes[i].node_handle = -1;
    }
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
            uint16_t msgsz = sh_from_littleendian16((uint8_t*)buf.ptr) + 2;
            if (msgsz <= 6 || (msgsz-6) > UM_MAXSZ) {
                err = NET_ERR_MSG;
                break;
            }
            if (buf.sz < msgsz) {
                break;
            }
            uint16_t source = sh_from_littleendian16((uint8_t*)buf.ptr+2);
            uint16_t dest = sh_from_littleendian16((uint8_t*)buf.ptr+4);
            int type = (dest>>8) & 0xff;
            dest &= 0xff;
            //sc_debug("+++++++READ: source %04x dest %04x type %d sz %d", source, dest, type, msgsz-6);
            sh_service_send(source, dest, type, buf.ptr+6, msgsz-6);
            buf.ptr += msgsz;
            buf.sz  -= msgsz;
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
node_send(struct service *s, int session, int source, int dest, int type, const void *msg, int sz) {
    struct remote *self = SERVICE_SELF;
    _send(self, source, dest, type, msg, sz);
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
    case NETE_CONNECT:
        sc_info("connect to node ok, %d", nm->connid);
        break;
    case NETE_CONNERR:
        sc_error("connect to node fail: %s", sc_net_error(nm->error));
        break;
    case NETE_SOCKERR:
        sc_error("node disconnect: %s, %d", sc_net_error(nm->error), nm->connid);
        _disconnect_node(self, nm->connid);
        break;
    }
}

void
node_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    if (type != MT_TEXT) {
        return;
    }
    /*
    char tmp[sz+1];
    if (tmp == NULL) {
        sc_error("Invalid command");
        return;
    }
    memcpy(tmp, msg, sz);
    tmp[sz] = '\0';
    sc_info("[NODE] %s", tmp);
    */
    struct remote *self = SERVICE_SELF;
    struct args A;
    if (args_parsestrl(&A, 0, msg, sz) < 1)
        return;

    const char *cmd = A.argv[0];

    if (!strcmp(cmd, "REG")) {
        if (A.argc != 7)
            return;
        int id = strtol(A.argv[1], NULL, 10);
        const char *naddr = A.argv[2];
        int nport = strtol(A.argv[3], NULL, 10);
        const char *gaddr = A.argv[4];
        int gport = strtol(A.argv[5], NULL, 10);
        int node_handle = strtol(A.argv[6], NULL, 10);

        struct _node *no = _get_node(self, id);
        if (no) {
            _update_node(no, naddr, nport, gaddr, gport);
            _bound_node_entry(no, node_handle);
            _connect_node(s, no);
        }
    } else if (!strcmp(cmd, "ADDR")) {
        if (A.argc != 6)
            return;
        int id = strtol(A.argv[1], NULL, 10);
        const char *naddr = A.argv[2];
        int nport = strtol(A.argv[3], NULL, 10);
        const char *gaddr = A.argv[4];
        int gport = strtol(A.argv[5], NULL, 10);
        struct _node *no = _get_node(self, id);
        if (no) {
            _update_node(no, naddr, nport, gaddr, gport);
        }
    } else if (!strcmp(cmd, "BROADCAST")) {
        if (A.argc != 2)
            return;
        int id = strtol(A.argv[1], NULL, 10);
        _broadcast_node(s, id);
    } else if (!strcmp(cmd, "SUB")) {
        if (A.argc != 2)
            return;
        sh_service_send(SERVICE_ID, self->center_handle, MT_TEXT, msg, sz);
    } else if (!strcmp(cmd, "PUB")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        char *p = strchr(name, ':');
        if (p) {
            p[0] = '\0';
            int handle = strtol(p+1, NULL, 16); 
            _publish_service(s, name, handle);
        }
    } else if (!strcmp(cmd, "HANDLE")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        char *p = strchr(name, ':');
        if (p) {
            p[0] = '\0';
            int handle = strtol(p+1, NULL, 16); 
            _connect_service(s, name, handle);
        }
    }
}
