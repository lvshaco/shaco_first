#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"

#define UNIQUE_INIT 1

struct shandle {
    int handle;
    bool sync;
};

struct uniqueol {
    int requester_handle;
    struct sh_hash uni;
    bool ready;
    bool befores_starting;
    int cap;
    int sz;
    struct shandle *befores;
};

static void
shandle_fini(struct uniqueol *self) {
    if (self->befores) {
        free(self->befores);
        self->befores = NULL;
        self->cap = 0;
        self->sz = 0;
    }
}

static void
shandle_add(struct uniqueol *self, int handle) {
    int i;
    for (i=0; i<self->sz; ++i) {
        if (self->befores[i].handle == handle)
            return;
    }
    if (self->sz == self->cap) {
        self->cap *= 2;
        if (self->cap == 0)
            self->cap = 1;
        self->befores = realloc(self->befores, sizeof(self->befores[0]) * self->cap);
    }
    struct shandle *h = &self->befores[self->sz++];
    h->handle = handle;
    h->sync = false;
}

static void
shandle_rm(struct uniqueol *self, int handle) {
    int i;
    for (i=0; i<self->sz; ++i) {
        if (self->befores[i].handle == handle) {
            for (; i<self->sz-1; ++i) {
                self->befores[i] = self->befores[i+1];
            }
            return;
        }
    }
}

static bool
shandle_syncok(struct module *s, int handle) {
    sh_trace("Uniqueol handle %04x syncok", handle);
    struct uniqueol *self = MODULE_SELF;
    struct shandle *h;
    int i;
    for (i=0; i<self->sz; ++i) {
        h = &self->befores[i];
        if (h->handle == handle) {
            h->sync = true;
            return true;
        }
    }
    return false;
}

static void
shandle_check_ready(struct module *s) {
    struct uniqueol *self = MODULE_SELF;
    struct shandle *h;
    int i;
    for (i=0; i<self->sz; ++i) {
        h = &self->befores[i];
        if (!h->sync)
            return;
    }
    sh_trace("Uniqueol all before handles syncok -> broadcast ready");
    self->ready = true;
    shandle_fini(self);

    UM_DEFFIX(UM_UNIQUEREADY, ready);
    sh_handle_broadcast(MODULE_ID, self->requester_handle, MT_UM, ready, sizeof(*ready));
}

struct uniqueol *
uniqueol_create() {
    struct uniqueol *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
uniqueol_free(struct uniqueol *self) {
    if (self == NULL)
        return;

    shandle_fini(self);
    sh_hash_fini(&self->uni);
    free(self);
}

int
uniqueol_init(struct module *s) {
    struct uniqueol *self = MODULE_SELF;

    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    struct sh_monitor h = { MODULE_ID, MODULE_ID };
    const char *requester = sh_getstr("uniqueol_requester", "");
    if (sh_handle_monitor(requester, &h, &self->requester_handle)) {
        return 1;
    }
    sh_hash_init(&self->uni, UNIQUE_INIT);
    return 0;
}

static void
umsg(struct module *s, int source, const void *msg, int sz) {
    struct uniqueol *self = MODULE_SELF;
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_UNIQUEUSE: {
        UM_CAST(UM_UNIQUEUSE, use, base);
        int status;
        if (!sh_hash_insert(&self->uni, use->id, (void*)(intptr_t)source)) {
            status = UNIQUE_USE_OK;
        } else {
            status = UNIQUE_HAS_USED;
        }
        UM_DEFFIX(UM_UNIQUESTATUS, st);
        st->id = use->id;
        st->status = status;
        sh_handle_send(MODULE_ID, source, MT_UM, st, sizeof(*st));
        }
        break;
    case IDUM_UNIQUEUNUSE: {
        UM_CAST(UM_UNIQUEUNUSE, unuse, base);
        sh_hash_remove(&self->uni, unuse->id);
        } 
        break;
    case IDUM_SYNCOK:
        if (shandle_syncok(s, source)) {
            shandle_check_ready(s);
        }
        break;
    }
}

struct exitud {
    struct uniqueol *self;
    int source;
};

static void
exitcb(uint32_t key, void *pointer, void *ud) {
    struct exitud *ed = ud;
    struct uniqueol *self = ed->self;
    if ((int)(intptr_t)pointer == ed->source) {
        sh_hash_remove(&self->uni, key);
    }
}

static void
monitor(struct module *s, int source, const void *msg, int sz) {
    struct uniqueol *self = MODULE_SELF;
    assert(sz >= 5);
    int type = sh_monitor_type(msg);
    int vhandle = sh_monitor_vhandle(msg);
    switch (type) {
    case MONITOR_STARTB:
        sh_trace("Uniqueol befores_starting ...");
        self->befores_starting = true;
        break;
    case MONITOR_STARTE:
        self->befores_starting = false;
        sh_trace("Uniqueol befores_starting ok");
        shandle_check_ready(s);
        break;
    case MONITOR_START:
        if (vhandle == self->requester_handle) {
            if (self->befores_starting) {
                sh_trace("Uniqueol befores_staring add handle %04x", source);
                shandle_add(self, source);
            }
            if (self->ready) {
                sh_trace("Uniqueol notify ready to handle %04x", source);
                UM_DEFFIX(UM_UNIQUEREADY, ready);
                sh_handle_send(MODULE_ID, source, MT_UM, ready, sizeof(*ready));
            }
        }
        break;
    case MONITOR_EXIT:
        if (vhandle == self->requester_handle) {
            struct exitud ud = { self, source };
            sh_hash_foreach3(&self->uni, exitcb, &ud);
            if (!self->ready) {
                sh_trace("Uniqueol rm handle %04x", source);
                shandle_rm(self, source);
            }
        }
        break;
    }
}

static int
user(struct module *s, struct args *A, struct memrw *rw) {
    struct uniqueol *self = MODULE_SELF;
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    int n;
    uint32_t accid = strtoul(A->argv[1], NULL, 10);
    int source = (int)(intptr_t)sh_hash_find(&self->uni, accid);
    if (source > 0) {
        n = snprintf(rw->ptr, RW_SPACE(rw), "source(%04x)", source);
    } else {
        n = snprintf(rw->ptr, RW_SPACE(rw), "none");
    }
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    //struct uniqueol *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "user")) {
        return user(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

void
uniqueol_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM:
        umsg(s, source, msg, sz);
        break;
    case MT_MONITOR:
        monitor(s, source, msg, sz);
        break;
    case MT_CMD:
        cmdctl(s, source, msg, sz, command);
        break;
    }
}
