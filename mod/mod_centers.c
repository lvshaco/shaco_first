#include "sh.h"
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
    struct _pubsub_slot *slot;
    int i;
    for (i=0; i<self->ps.sz; ++i) {
        slot = &self->ps.p[i];
        free(slot->pubs.p);
        free(slot->subs.p);
    }
    free(self->ps.p);
    self->ps.p = NULL;
    self->ps.sz = 0;
    self->ps.cap = 0;
    free(self);
}

int
centers_init(struct module* s) {
    struct centers *self = MODULE_SELF;
    if (sh_handler("node", SUB_LOCAL, &self->node_handle)) {
        return 1;
    }
    return 0;
}

static struct _pubsub_slot *
insert_pubsub_name(struct _pubsub_array *ps, const char *name) {
    int i;
    for (i=0; i<ps->sz; ++i) {
        if (!strcmp(ps->p[i].name, name))
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
    sh_strncpy(slot->name, name, sizeof(slot->name));
    return slot;
}

static int
insert_int(struct _int_array *inta, int value) {
    if (value < 0)
        return 1;

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

static void
remove_int(struct _int_array *inta, int nodeid) {
    int i,j;
    for (i=0; i<inta->sz; ++i) {
        if (sh_nodeid_from_handle(inta->p[i]) == nodeid) {
            for (j=i; j<inta->sz-1; ++j) {
                inta->p[j] = inta->p[j+1];
            }
            inta->sz--;
        }
    }
}

static void
unreg_node(struct centers *self, int nodeid) {
    struct _pubsub_array* ps = &self->ps;
    int i;
    for (i=0; i<ps->sz; ++i) {
        remove_int(&ps->p[i].pubs, nodeid);
        remove_int(&ps->p[i].subs, nodeid);
    }
}

void
centers_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    if (type != MT_TEXT) {
        return;
    }
    struct centers *self = MODULE_SELF;
    struct args A;
    if (args_parsestrl(&A, 0, msg, sz) < 1)
        return;
    
    const char *cmd = A.argv[0];

    if (!strcmp(cmd, "REG")) {
        sh_module_send(MODULE_ID, self->node_handle, MT_TEXT, msg, sz);
        int nodeid = strtol(A.argv[1], NULL, 10);
        sh_module_vsend(MODULE_ID, self->node_handle, "BROADCAST %d", nodeid);
    } else if (!strcmp(cmd, "UNREG")) {
        if (A.argc != 2)
            return;
        int nodeid = strtol(A.argv[1], NULL, 10);
        unreg_node(self, nodeid);
    } else if (!strcmp(cmd, "SUB")) {
        if (A.argc != 2)
            return;
        const char *name = A.argv[1];
        struct _pubsub_slot *slot = insert_pubsub_name(&self->ps, name);
        if (slot == NULL)
            return;
        if (insert_int(&slot->subs, source))
            return;
        int i, n = 0;
        char tmp[2048];
        n += sh_snprintf(tmp, sizeof(tmp), "HANDLES %s:", name);
        for (i=0; i<slot->pubs.sz; ++i) {
            int pub_handle = slot->pubs.p[i];
            if (sh_nodeid_from_handle(pub_handle) !=
                sh_nodeid_from_handle(source)) {
                assert(sizeof(tmp) > n);
                assert(sizeof(tmp) - n > 8);
                n += sh_snprintf(tmp+n, sizeof(tmp)-n, "%04x,", slot->pubs.p[i]);
            }
        }
        tmp[--n] = '\0';
        sh_module_send(MODULE_ID, source, MT_TEXT, tmp, n);
    } else if (!strcmp(cmd, "PUB")) {
        if (A.argc != 2)
            return;
        char *p = strchr(A.argv[1], ':');
        if (p == NULL)
            return;
        *p = '\0';
        const char *name = A.argv[1];
        int handle = strtol(p+1, NULL, 16);
        struct _pubsub_slot *slot = insert_pubsub_name(&self->ps, name);
        if (slot == NULL)
            return;
        if (insert_int(&slot->pubs, handle))
            return;
        int i;
        for (i=0; i<slot->subs.sz; ++i) {
            int sub_handle = slot->subs.p[i];
            if (sh_nodeid_from_handle(sub_handle) !=
                sh_nodeid_from_handle(source)) {
                sh_module_vsend(MODULE_ID, slot->subs.p[i],
                        "HANDLE %s:%04x", name, handle);
            }
        }
    }
}
