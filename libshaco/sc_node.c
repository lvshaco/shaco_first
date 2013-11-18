#include "sc_node.h"
#include "sc_init.h"
#include "sc_log.h"
#include "sc_net.h"
#include <limits.h>
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
    int loaditer;
    struct sc_node* p;
};

struct _node_holder {
    uint16_t me; 
    int size;
    struct _type* types;
    struct _array* nodes;
};

static struct _node_holder* N = NULL;

int
sc_node_typeid(const char* name) {
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
sc_node_typename(uint16_t tid) {
    if (tid >= 0 && tid < N->size) {
        return N->types[tid].name;
    }
    return "";
}

int  
sc_node_types() {
    return N->size;
}
int 
sc_node_register_types(const char* types[], int n) {
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
_isme(struct sc_node* node) {
    return node->id == N->me;
}

static inline bool
_equal_node(struct sc_node* a, struct sc_node* b) {
    return a->id == b->id &&
        a->addr == b->addr &&
        a->port == b->port &&
        a->gaddr == b->gaddr &&
        a->gport == b->gport &&
        a->connid == b->connid;
}

static inline void
_init_node(struct sc_node* node) {
    node->id = -1;
    node->addr = 0;
    node->port = 0;
    node->gaddr = 0;
    node->gport = 0;
    node->connid = -1;
    node->load = 0;
}

static inline void
_free_node(struct sc_node* node) {
    if (!_isme(node)) {
        _init_node(node);
    }
}

static inline bool
_isfree_node(struct sc_node* node) {
    return (!_isme(node)) && node->connid == -1;
}

static int
_add_node(struct _array* arr, struct sc_node* node) {
    int idx = HNODE_SID(node->id);
    int cap = arr->cap;
    if (idx >= cap) {
        if (cap <= 0)
            cap = 1;
        while (cap <= idx) {
            cap *= 2;
        }
        arr->cap = cap;
        arr->p = realloc(arr->p, sizeof(struct sc_node) * cap); 
        int i;
        for (i=idx; i<cap; ++i) {
            _init_node(&arr->p[i]);
        }
    }
    struct sc_node* c = &arr->p[idx];
    if (_isfree_node(c)) {
        *c = *node;
        c->load = 0;
        arr->size = idx + 1;
        return 0;
    }
    if (_equal_node(node, c))
        return 0;
    return 1;
}

static inline struct sc_node*
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

const char*
sc_strnode(const struct sc_node* node, char str[HNODESTR_MAX]) {
    uint16_t tid = HNODE_TID(node->id);
    uint16_t sid = HNODE_SID(node->id);
    const char* type;
    if (tid >= 0 && tid < N->size) {
        type = N->types[tid].name;
    } else {
        type = "";
    }
    struct in_addr in;
    uint32_t laddr = 0;
    uint16_t lport = 0;
    sc_net_socket_address(node->connid, &laddr, &lport);
    
    in.s_addr = laddr;
    char laddrs[24];
    strncpy(laddrs, inet_ntoa(in), sizeof(laddrs)-1);

    in.s_addr = node->addr;
    char naddrs[24];
    strncpy(naddrs, inet_ntoa(in), sizeof(naddrs)-1);

    in.s_addr = node->gaddr;
    char gaddrs[24];
    strncpy(gaddrs, inet_ntoa(in), sizeof(gaddrs)-1);

    snprintf(str, HNODESTR_MAX, "NODE[%s%04u,T%02u,C%04d,L%s:%u,N%s:%u,G%s:%u]",
        type, 
        sid,
        tid,
        node->connid,
        laddrs,
        lport,
        naddrs, 
        node->port,
        gaddrs, 
        node->gport);
    return str;
}

uint16_t 
sc_id() {
    return N->me;
}

struct sc_node* 
sc_me() {
    struct sc_node* me = _get_node(N->me);
    assert(me);
    assert(me->id == N->me);
    return me;
}

int 
sc_register_me(struct sc_node* me) {
    if (sc_node_register(me))
        return 1;
    N->me = me->id;
    return 0;
}

const struct sc_node* 
sc_node_get(uint16_t id) {
    struct sc_node* node = _get_node(id);
    if (node && node->id == id)
        return node;
    return NULL;
}

bool
sc_node_is_register(uint16_t id) {
    struct sc_node* node = _get_node(id);
    return node && node->id == id;
}

int 
sc_node_register(struct sc_node* node) {
    char strnode[HNODESTR_MAX];
    struct _array* arr;
    uint16_t tid = HNODE_TID(node->id);
    uint16_t sid = HNODE_SID(node->id);
    if (tid >= 0 && tid < N->size &&
        sid >= 0 && sid < HNODE_SID_MAX) {
        arr = &N->nodes[tid];
        if (_add_node(arr, node) == 0) {
            sc_info("node register, %s", sc_strnode(node, strnode));
            return 0;
        }
    } 
    sc_error("node register error, %s", sc_strnode(node, strnode));
    return 1;
}

int 
sc_node_unregister(uint16_t id) {
    struct sc_node* node = _get_node(id);
    if (node) {
        if (!_isfree_node(node)) {
            char strnode[HNODESTR_MAX];
            sc_info("node unregister, %s", sc_strnode(node, strnode));
            _free_node(node);
            return 0;
        }
    }
    return 1;
}

int 
sc_node_disconnect(int connid) {
    struct _array* arr;
    struct sc_node* node;
    int t;
    int i;
    for (t=0; t<N->size; ++t) {
        arr = &N->nodes[t];
        for (i=0; i<arr->size; ++i) {
            node = &arr->p[i];
            if (node->connid == connid) {
                char strnode[HNODESTR_MAX];
                sc_info("node disconnect, %s", sc_strnode(node, strnode));
                _free_node(node);
                return 0;
            }
        }     
    }
    return 1;
}

void 
sc_node_foreach(uint16_t tid, int (*cb)(const struct sc_node*, void* ud), void* ud) {
    struct _array* arr; 
    struct sc_node* node; 
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

const struct sc_node*  
sc_node_minload(uint16_t tid) {
    struct _array* arr;
    struct sc_node* node;
    
    int minload = INT_MAX;
    int sid = -1;
    int idx;
    int i;
    if (tid >= 0 && tid < N->size) {
        arr = &N->nodes[tid];
        for (i=0; i<arr->size; ++i) {
            idx = (arr->loaditer+i)%arr->size;
            node = &arr->p[idx];
            if (node->connid != -1) {
                if (node->load < minload) {
                    minload = node->load;
                    sid = idx;
                }
            }
        }
    }
    if (sid != -1) {
        arr->loaditer = sid+1;
        return _get_node(HNODE_ID(tid, sid));
    } else {
        return NULL;
    }
}

void 
sc_node_updateload(uint16_t id, int value) {
    struct sc_node* node = _get_node(id);
    if (node) {
        node->load += value;
    }
}

void 
sc_node_setload(uint16_t id, int value) {
    struct sc_node* node = _get_node(id);
    if (node) {
        node->load = value;
    }
}

static void
sc_node_init() {
    N = malloc(sizeof(*N));
    memset(N, 0, sizeof(*N));
    //N->me = -1;
}

static void
sc_node_fini() {
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

SC_LIBRARY_INIT_PRIO(sc_node_init, sc_node_fini, 25)
