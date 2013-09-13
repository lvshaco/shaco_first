#include "host_node.h"
#include "host_log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>

#define NODE_NAME_MAX 16

struct _type {
    char name[NODE_NAME_MAX];
};

struct _array {
    int cap;
    int size;
    struct host_node* p;
};

struct _node_holder {
    int size;
    struct _type* types;
    struct _array* nodes;
};

static struct _node_holder* N = NULL;

int 
host_node_init() {
    N = malloc(sizeof(*N));
    memset(N, 0, sizeof(*N));
    return 0;
}

void
host_node_free() {
    if (N== NULL)
        return;

    free(N->types);
    int i;
    struct _array* arr;
    for (i=0; i<N->size; ++i) {
        arr = &N->nodes[i];
        free(arr->p);
    }
    free(N->nodes);
    N->size = 0;
    free(N);
    N= NULL;
}

int
host_node_typeid(const char* name) {
    struct _type* t;
    int i;
    for (i=0; i<N->size; ++i) {
        t = &N->types[i];
        if (strcmp(t->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const char* 
host_node_typename(int id) {
    if (id >= 0 && id < N->size) {
        return N->types[id].name;
    }
    return "";
}

int 
host_node_register_types(const char* types[], int n) {
    if (N->size > 0)
        return -1;
    N->size = n;
    N->types = malloc(sizeof(struct _type) * n);
    memset(N->types, 0, sizeof(struct _type) * n);
    N->nodes = malloc(sizeof(struct _array) * n);
    memset(N->nodes, 0, sizeof(struct _array) * n);

    struct _type* t;
    int i;
    for (i=0; i<n; ++i) {
        t = &N->types[i];
        strncpy(t->name, types[i], NODE_NAME_MAX-1);
    }
    return 0;
}

static void
_free_node(struct host_node* node) {
    node->tid = 0;
    node->sid = 0;
    node->addr = 0;
    node->port = 0;
    node->connid = -1;
}

static inline bool
_isfree_node(struct host_node* node) {
    return node->connid == -1;
}


static int
_add_node(struct _array* arr, struct host_node* node) {
    int i = node->sid;
    int cap = arr->cap;
    if (i >= cap) {
        if (cap <= 0)
            cap = 1;
        while (cap <= i) {
            cap *= 2;
        }
        arr->cap = cap;
        arr->p = realloc(arr->p, sizeof(struct host_node) * cap);
        memset(arr->p + i, 0, sizeof(struct host_node) * (cap - i));
    }
    struct host_node* fill = &arr->p[i];
    if (_isfree_node(fill)) {
        *fill = *node;
        arr->size = i + 1;
        return 0;
    }
    return 1;
}

#define NODESTR_MAX 128

static const char*
_strnode(struct host_node* node, char str[NODESTR_MAX]) {
    uint16_t tid = node->tid;
    const char* type;
    if (tid >= 0 && tid < N->size) {
        type = N->types[tid].name;
    } else {
        type = "";
    }
    struct in_addr in;
    in.s_addr = node->addr;
    snprintf(str, NODESTR_MAX, "[type:%s tid:%d sid:%d %s:%d]",
        type, 
        node->tid, 
        node->sid, 
        inet_ntoa(in), 
        node->port);
    return str;
}

int 
host_node_register(struct host_node* node) {
    struct _array* arr;
    uint16_t tid = node->tid;
    uint16_t sid = node->sid;
    if (tid >= 0 && tid < N->size &&
        sid >= 0 && sid < HOST_NODE_SUBID_MAX) {
        arr = &N->nodes[tid];
        return _add_node(arr, node);
    }
    char strnode[NODESTR_MAX];
    host_error("host node register error, %s", _strnode(node, strnode));
    return 1;
}

int 
host_node_unregister(uint16_t tid, uint16_t sid) {
    struct _array* arr;
    struct host_node* node;
    int i = sid;
    if (tid >= 0 && tid < N->size) {
        arr = &N->nodes[tid];
        if (i >= 0 && i < arr->size) {
            node = &arr->p[i];
            if (!_isfree_node(node)) {
                char strnode[NODESTR_MAX];
                host_debug("host node unregister, %s", _strnode(node, strnode));
                _free_node(node);
                return 0;
            }
        }
    }
    return 1;
}

int 
host_node_disconnect(int connid) {
    struct _array* arr;
    struct host_node* node;
    int t;
    int i;
    for (t=0; t<N->size; ++t) {
        arr = &N->nodes[t];
        for (i=0; i<arr->size; ++i) {
            node = &arr->p[i];
            if (node->connid == connid) {
                char strnode[NODESTR_MAX];
                host_debug("host node disconnect, %s", _strnode(node, strnode));
                _free_node(node);
                return 0;
            }
        }     
    }
    return 1;
}
