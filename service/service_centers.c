#include "sc_service.h"
#include "sc_env.h"
#include "sc_node.h"
#include "sc_log.h"
#include "sc_util.h"
#include "args.h"

struct _int_array {
    int cap;
    int sz;
    int *p;     
};

struct _pubsub_slot {
    char name[32]; 
    struct _int_array pubs;
    struct _int_array subs;
};

struct _pubsub_array {
    int cap;
    int sz;
    struct _pubsub_slot *p;
};

struct centers { 
    int node_handle;
    struct _pubsub_array ps;
};

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
    free(self);
}

int
centers_init(struct service* s) {
    struct centers *self = SERVICE_SELF;
    self->node_handle = sc_service_subscribe("node");
    if (self->node_handle == -1)
        return 1;
    return 0;
}

static struct _pubsub_slot *
_insert_pubsub_name(struct _pubsub_array *ps, const char *name) {
    int i;
    for (i=0; i<ps->sz; ++i) {
        if (strcmp(ps->p[i].name, name) == 0)
            return &ps->p[i];
    }
    if (ps->sz >= ps->cap) {
        ps->cap *= 2;
        if (ps->cap == 0)
            ps->cap = 1;
        ps->p = realloc(ps->p, sizeof(struct _pubsub_slot) * ps->cap);
        memset(ps->p+ps->sz, 0, sizeof(struct _pubsub_slot) * (ps->cap-ps->sz));
    }
    struct _pubsub_slot *slot = &ps->p[ps->sz++];
    sc_strncpy(slot->name, name, sizeof(slot->name));
    return slot;
}

static int
_insert_int(struct _int_array *inta, int value) {
    int i;
    for (i=0; i<inta->sz; ++i) {
        if (inta->p[i] == value)
            return 1;
    }
    if (inta->sz >= inta->cap) {
        inta->cap *= 2;
        if (inta->cap == 0)
            inta->cap = 1;
        inta->p = realloc(inta->p, sizeof(int) * inta->cap);
        memset(inta->p+inta->sz, 0, sizeof(int) * (inta->cap-inta->sz));
    }
    inta->p[inta->sz++] = value;
    return 0;
}

void
centers_main(struct service *s, int session, int source, const void *msg, int sz) {
    struct centers *self = SERVICE_SELF;
    struct args A;
    char tmp[sz+1];
    memcpy(tmp, msg, sz);
    tmp[sz]='\0';
    sc_debug(tmp);

    if (args_parsestrl(&A, 0, msg, sz) < 1)
        return;
    const char *cmd = A.argv[0];

    if (strcmp(cmd, "REG")) {
        sc_service_send(SERVICE_ID, self->node_handle, msg, sz);
        int nodeid = strtol(A.argv[1], NULL, 10);
        sc_service_send(SERVICE_ID, self->node_handle, "BROADCAST %d", nodeid);
    } else if (strcmp(cmd, "SUB")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        struct _pubsub_slot *slot = _insert_pubsub_name(&self->ps, name);
        if (slot == NULL)
            return;
        int nodeid = sc_nodeid_from_handle(source);
        if (_insert_int(&slot->subs, nodeid)) {
            sc_error("Subscribe %s repeat by node#%d", name, nodeid);
            return;
        }
        int i;
        for (i=0; i<slot->pubs.sz; ++i) {
            sc_service_vsend(SERVICE_ID, source, 
                    "HANDLE %s:%d", name, slot->pubs.p[i]);
        }
    } else if (strcmp(cmd, "PUB")) {
        if (A.argc != 2)
            return;
        char *p = strchr(A.argv[1], ':');
        if (p == NULL)
            return;
        *p = '\0';
        const char *name = A.argv[1];
        int handle = strtol(p+1, NULL, 10);
        struct _pubsub_slot *slot = _insert_pubsub_name(&self->ps, name);
        if (slot == NULL)
            return;
        int i;
        for (i=0; i<slot->subs.sz; ++i) {
            sc_service_vsend(SERVICE_ID, slot->subs.p[i],
                    "HANDLE %s:%d", name, handle);
        }
    }
}
