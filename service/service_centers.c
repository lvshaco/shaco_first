#include "host_service.h"
#include "host_node.h"
#include "host_log.h"
#include "host_dispatcher.h"
#include "node_type.h"
#include "user_message.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct _array {
    int cap;
    int size;
    uint16_t* p;
};

struct centers {
    struct _array subs[NODE_TYPE_MAX];
};

static void
_add_subscribe(struct _array* arr, uint16_t tid) {
    int tmp;
    int i;
    for (i=0; i<arr->size; ++i) {
        tmp = arr->p[i];
        if (tid == tmp)
            return; // already sub
    }
    int idx = arr->size;
    int cap = arr->cap;
    if (idx >= cap) {
        cap *= 2;
        if (cap <= 0)
            cap = 1;
        arr->p = realloc(arr->p, sizeof(arr->p[0]) * cap);
        arr->cap = cap;
        memset(arr->p + idx, 0, sizeof(arr->p[0] * (cap-idx)));
    }
    arr->p[idx] = tid;
    arr->size = idx+1;
}

struct centers*
centers_create() {
    struct centers* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
centers_free(struct centers* self) {
    if (self == NULL)
        return;
    
    struct _array* arr;
    int i;
    for (i=0; i<NODE_TYPE_MAX; ++i) {
        arr = &self->subs[i];
        free(arr->p);
    }
    free(self);
}

int
centers_init(struct service* s) {
    SUBSCRIBE_MSG(s->serviceid, IDUM_NODESUBS);
    return 0;
}

static inline bool
_isvalid_tid(uint16_t tid) {
    return tid < NODE_TYPE_MAX && tid != NODE_CENTER;
}

static void
_notify(int id, const struct host_node* node) {
    UM_DEFFIX(UM_NODENOTIFY, notify);
    notify->tnodeid = node->id;
    notify->addr = node->addr;
    notify->port = node->port;
    UM_SEND(id, notify, sizeof(*notify));
}

static int
_subscribecb(const struct host_node* node, void* ud) {
    int id = (int)(intptr_t)ud;
    _notify(id, node);
    return 0;
}

static void
_subscribe(struct centers* self, int id, struct UM_BASE* um) {
    UM_CAST(UM_NODESUBS, req, um);
    uint16_t src_tid = HNODE_TID(req->nodeid);
    uint16_t tid;
    struct _array* arr;
    int i;
    for (i=0; i<req->n; ++i) {
        tid = req->subs[i];
        if (!_isvalid_tid(tid)) {
            host_error("subscribe fail: invalid tid:%d,%s", 
                    tid, host_node_typename(tid));
            continue;
        }
        arr = &self->subs[tid];
        _add_subscribe(arr, src_tid);
        host_node_foreach(tid, _subscribecb, (void*)(intptr_t)id);
    }
}

static int
_onregcb(const struct host_node* node, void* ud) {
    struct host_node* tnode = ud;
    _notify(node->connid, tnode);
    return 0;
}

static void
_onreg(struct centers* self, struct host_node* node) {
    struct host_node* tnode = node;
    uint16_t tid = HNODE_TID(node->id);
    assert(_isvalid_tid(tid));

    struct _array* arr = &self->subs[tid];
    uint16_t sub;
    int i;
    for (i=0; i<arr->size; ++i) {
        sub = arr->p[i];
        host_node_foreach(sub, _onregcb, tnode); 
    }
}

void
centers_service(struct service* s, struct service_message* sm) {
    struct centers* self = SERVICE_SELF;
    struct host_node* regn = sm->msg;
    _onreg(self, regn);
}

void
centers_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct centers* self = SERVICE_SELF;
    struct UM_BASE* um = msg;
    switch (um->msgid) {
    case IDUM_NODESUBS:
        _subscribe(self, id, um);
        break;
    }
}
