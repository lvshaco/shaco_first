#include "host_service.h"
#include "host_net.h"
#include "host_log.h"
#include "host.h"
#include "stringtable.h"
#include "array.h"
/*
#define NODE_MAX 256

struct _node {
    bool used;
    int connection;
    const char* name;
    uint32_t ip;
    uint16_t port;
    struct array subscribes;
};

struct _center {
    struct stringtable* names;
    struct _node* nodes;
    int nnode;
};

static void
_node_fini(struct _node* n) {
    if (!n->used)
        return;
    array_fini(n->subscribes);
    n->used = false;
}

struct _center*
center_create() {
    struct _center* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
center_free(struct _center* self) {
    if (self == NULL)
        return;

    if (self->names) {
        stringtable_free(self->names);
        self->names = NULL;
    }
    if (self->nodes) {
        struct _node* n;
        int i;
        for (i=0; i<self->nnode; ++i) {
            n = &self->nodes[i];
            _node_fini(n); 
        }
        free(self->nodes);
        self->nodes = NULL;
        self->nnode = 0;
    }
    free(self);
}

int
center_init(struct service* s) {
    struct _center* self = SERVICE_SELF;
        
    int hashcap = 1;
    while (hashcap < max)
        hashcap *= 2;

    if (self->names == NULL) {
        self->names = stringtable_create(128);
    }
    if (self->nodes == NULL) {
        self->nodes == malloc(sizeof(struct _node) * NODE_MAX);
        self->nnode = NODE_MAX;
    }

    if (host_net_listen(host_getstr("center_ip", ""),
                        host_getint("center_port", 0),
                        s->serviceid)) {
        return 1;
    }
    return 0;
}

static void
_create_node(struct _center* self, int connection) {
    struct _node* newn = NULL;
    struct _node* n;
    int i;
    for (i=0; i<self->nnode; ++i) {
        n = &self->nodes[i];
        if (!n->used) {
            n->used = true;
            newn = n;
            break;
        }
    }
    if (newn == NULL) {
        host_error("node has reach max");
        return;
    }
    newn->connection = nm->connid;
}

static void
_free_node(struct _center* self, int connection) {
    struct _node* n;
    int i;
    for (i=0; i<self->nnode; ++i) {
        n = &self->nodes[i];
        if (n->used && 
            n->connection == connection) {
            _node_fini(n);
            return;
        }
    }
}

static struct _node*
_find_node(struct _center* self, int connection) {
    struct _node* n;
    int i;
    for (i=0; i<self->nnode; ++i) {
        n = &self->nodes[i];
        if (n->used &&
            n->connection == connection) {
            return n;
        }
    }
    return NULL;
}

static struct _node*
_find_node_byname(struct _center* self, const char* name) {
    struct _node* n = NULL;
    int i;
    for (i=0; i<self->nnode; ++i) {
        n = &self->nodes[i];
        if (n->used &&
            n->name == name) {
            return n;
        }
    }
    return NULL;
}

static void
_notify_connect(struct _node* me, struct _node* tar) {
    UM_DEF(um, 128);
    um->sz = snprintf(um->data, sizeof(um->data), "CON name=%s ip=%u port=%u",
            tar->name, tar->ip, tar->port);
    um->sz += 1;
    host_net_send(me->connection, um, UM_SIZE(um));
}

static bool
_is_subscribe(struct _node* n, const char* name) {
    const char* tmp = NULL;
    int i;
    for (i=0; i<array_size(&n->subscribes); ++i) {
        tmp = array_get(&n->subscribes, i);
        if (tmp == name) {
            return true;
        }
    }
    return false;
}

static void
_notify(struct _center* self, struct _node* me) {
    struct _node* n = NULL;
    int i;
    for (i=0; i<array_size(&n->subscribes); ++i) {
        const char* name = array_get(&n->subscribes, i);
        if (name == n->name)
            continue;
        
        n = _find_node_byname(self, name);
        if (n) 
            _notify_connect(me, n);
    }
    
    int i;
    for (i=0; i<self->nnode; ++i) {
        n = &self->nodes[i];
        if (n->used &&
            n->connection != me->connection &&
            _is_subscribe(n, me->name)) {
            _notify_connect(n, me);
        }
    }
}

static void
_handle_message(struct _center* self, struct _node* n, struct user_message* um) {
    um->data[um->sz] = '\0';
    if (memcmp("REG", um->data, 3) == 0) {
        char name[16];
        uint32_t ip;
        int port; 
        char subscribe[16*10+10];
        sscanf(um->data+4, "name=%s ip=%u port=%u connect=%s", 
                name, ip, &port, subscribe);
        n->name = stringtable_str(self->names, name);
        n->ip = ip;
        n->port = port;
        array_reset(&self->subscribes);
        string2array_st(subscribe, ',', &self->subscribes, self->names);
    }
}

static void
_read(struct _center* self, int id) {
    struct _node* n = _find_node(self, id);
    if (n == NULL) {
        host_error("no found node %d", id);
        return;
    }
    const char* error;
    struct user_message* um = user_message_read(id, &error);
    while (um) {
        _handle_message(self, n, um);
        host_net_dropread(id);
        um = user_message_read(id, &error);
    }
    if (!NET_OK(error)) {
        _free_node(self, n);
    }
}

void
center_net(struct service* s, struct net_message* nm) {
    struct _center* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm->connid);
        break;
    case NETE_ACCEPT:
        _create_node(self, nm->connid);
        break;
    }
}
*/
