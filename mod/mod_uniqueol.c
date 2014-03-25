#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"

#define UNIQUE_INIT 1

struct uniqueol {
    int requester_handle;
    struct sh_hash uni;
};

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
    
    sh_hash_fini(&self->uni);
    free(self);
}

int
uniqueol_init(struct module *s) {
    struct uniqueol *self = MODULE_SELF;

    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    struct sh_monitor_handle h = { -1, MODULE_ID };
    const char *requester = sh_getstr("uniqueol_requester", "");
    if (sh_monitor(requester, &h, &self->requester_handle)) {
        return 1;
    }
    sh_hash_init(&self->uni, UNIQUE_INIT);
    return 0;
}

static void
process_unique(struct module *s, int source, struct UM_BASE *base) {
    struct uniqueol *self = MODULE_SELF;

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
        sh_module_send(MODULE_ID, source, MT_UM, st, sizeof(*st));
        }
        break;
    case IDUM_UNIQUEUNUSE: {
        UM_CAST(UM_UNIQUEUNUSE, unuse, base);
        sh_hash_remove(&self->uni, unuse->id);
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
    case MONITOR_EXIT:
        if (vhandle == self->requester_handle) {
            struct exitud ud = { self, source };
            sh_hash_foreach3(&self->uni, exitcb, &ud);
        }
        break;
    }
}

void
uniqueol_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        process_unique(s, source, base);
        }
        break;
    case MT_MONITOR:
        monitor(s, source, msg, sz);
        break;
    case MT_CMD:
        cmdctl(s, source, msg, sz, NULL);
        break;
    }
}
