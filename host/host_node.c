#include "host_node.h"
#include "host_log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>

struct _type {
    char name[HNODE_NAME_MAX];
};

struct _array {
    int cap;
    int size;
    struct host_node* p;
};

struct _node_holder {
    uint16_t me; 
    int size;
    struct _type* types;
    struct _array* nodes;
};

static struct _node_holder* N = NULL;

int 
host_node_init() {
    N = malloc(sizeof(*N));
    memset(N, 0, sizeof(*N));
    N->me = -1;
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

//bool
//host_node_istype(int tid) {
    //return tid >=0 && tid < N->size;
//}

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
host_node_typename(uint16_t tid) {
    if (tid >= 0 && tid < N->size) {
        return N->types[tid].name;
    }
    return "";
}

int 
host_node_register_types(const char* types[], int n) {
    if (N->size > 0)
        return 1;
    if (n > HNODE_TID_MAX)
        return 1;
    N->size = n;
    N->types = malloc(sizeof(struct _type) * n);
    memset(N->types, 0, sizeof(struct _type) * n);
    N->nodes = malloc(sizeof(struct _array) * n);
    memset(N->nodes, 0, sizeof(struct _array) * n);

    struct _type* t;
    int i;
    for (i=0; i<n; ++i) {
        t = &N->types[i];
        strncpy(t->name, types[i], HNODE_NAME_MAX-1);
    }
    return 0;
}

static bool
_isme(struct host_node* node) {
    return node->id == N->me;
}

static void
_free_node(struct host_node* node) {
    if (!_isme(node)) {
        node->id = 0;
        node->addr = 0;
        node->port = 0;
        node->connid = -1;
    }
}

static inline bool
_isfree_node(struct host_node* node) {
    return (!_isme(node)) && node->connid == -1;
}

static int
_add_node(struct _array* arr, struct host_node* node) {
    int i = HNODE_SID(node->id);
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
    struct host_node* c = &arr->p[i];
    if (_isfree_node(c)) {
        *c = *node;
        arr->size = i + 1;
        return 0;
    }
    return 1;
}

static inline struct host_node*
_get_node(uint16_t id) {
    struct _array* arr;
    uint16_t tid = HNODE_TID(id);
    uint16_t sid = HNODE_SID(id);
    if (tid >= 0 && tid < N->size) {
        arr = &N->nodes[tid];
        if (sid >= 0 && sid < arr->size) {
            return &arr->p[sid];
        }
    }
    return NULL;
}

#define NODESTR_MAX 128

static const char*
_strnode(struct host_node* node, char str[NODESTR_MAX]) {
    uint16_t tid = HNODE_TID(node->id);
    uint16_t sid = HNODE_SID(node->id);
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
        tid, 
        sid, 
        inet_ntoa(in), 
        node->port);
    return str;
}

uint16_t 
host_id() {
    return N->me;
}

struct host_node* 
host_me() {
    struct host_node* me = _get_node(N->me);
    assert(me);
    assert(me->id == N->me);
    return me;
}

int 
host_register_me(struct host_node* me) {
    if (host_node_register(me))
        return 1;
    N->me = me->id;
    return 0;
}

struct host_node* 
host_node_get(uint16_t id) {
    struct host_node* node = _get_node(id);
    if (node && node->id == id)
        return node;
    return NULL;
}

bool
host_node_is_register(uint16_t id) {
    struct host_node* node = _get_node(id);
    return node && node->id == id;
}

int 
host_node_register(struct host_node* node) {
    char strnode[NODESTR_MAX];
    struct _array* arr;
    uint16_t tid = HNODE_TID(node->id);
    uint16_t sid = HNODE_SID(node->id);
    if (tid >= 0 && tid < N->size &&
        sid >= 0 && sid < HNODE_SID_MAX) {
        arr = &N->nodes[tid];
        if (_add_node(arr, node) == 0) {
            host_info("host node register, %s", _strnode(node, strnode));
            return 0;
        }
    } 
    host_error("host node register error, %s", _strnode(node, strnode));
    return 1;
}

int 
host_node_unregister(uint16_t id) {
    struct host_node* node = _get_node(id);
    if (node) {
        if (!_isfree_node(node)) {
            char strnode[NODESTR_MAX];
            host_info("host node unregister, %s", _strnode(node, strnode));
            _free_node(node);
            return 0;
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
                host_info("host node disconnect, %s", _strnode(node, strnode));
                _free_node(node);
                return 0;
            }
        }     
    }
    return 1;
}

void 
host_node_foreach(uint16_t tid, int (*cb)(struct host_node*, void* ud), void* ud) {
    struct _array* arr;
    struct hsot_node* node;
    int i;
    if (tid >= 0 && tid < N->size) {
        arr = &N->nodes[tid];
        for (i=0; i<arr->size; ++i) {
            node = &arr->p[i];
            if (node->connid != -1) {
                if (cb(node, ud)) {
                    return;
                }
            }
        }
    }
}
