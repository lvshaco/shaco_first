#include "sc_service.h"
#include "sc_node.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>

#define UNIQUE_INIT 1

struct unique {
    int n;
    uint8_t *p; 
};

struct uniqueol {
    int watchdog_handle;
    struct unique uni;
};

static void
unique_init(struct unique *uni, int init) {
    int n = 1;
    while (n < init)
        n *= 2;
    uni->n = n;
    uni->p = malloc(sizeof(uni->p[0]) * n);
    memset(uni->p, 0, sizeof(uni->p[0]) * n);
}

static void
unique_fini(struct unique *uni) {
    if (uni == NULL)
        return;
    free(uni->p);
}

static void
unique_use(struct unique *uni, uint32_t id) {
    uint32_t idx = id/8;
    uint32_t bit = id%8;
    if (idx >= uni->n) {
        int old = uni->n;
        while (uni->n <= idx) {
            uni->n *= 2;
        }
        uni->p = realloc(uni->p, sizeof(uni->p[0]) * uni->n);
        memset(uni->p + old, 0, sizeof(uni->p[0]) * (uni->n - old));
    }
    uni->p[idx] |= 1<<bit;
}

static inline int
unique_unuse(struct unique *uni, uint32_t id) {
    uint32_t idx = id/8;
    uint32_t bit = id%8;
    if (idx < uni->n) {
        uni->p[idx] &= ~(1<<bit);
        return 0;        
    }
    return 1;
}

static inline bool
unique_isuse(struct unique *uni, uint32_t id) {
    uint32_t idx = id/8;
    uint32_t bit = id%8;
    if (idx < uni->n) {
        return uni->p[idx] & (1<<bit);
    }
    return false;
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
    unique_fini(&self->uni);
    free(self);
}

int
uniqueol_init(struct service *s) {
    struct uniqueol *self = SERVICE_SELF;

    if (sc_handler("watchdog", &self->watchdog_handle)) {
        return 1;
    }
    unique_init(&self->uni, UNIQUE_INIT);
    return 0;
}

static void
process_unique(struct service *s, int source, struct UM_BASE *base) {
    struct uniqueol *self = SERVICE_SELF;

    switch (base->msgid) {
    case IDUM_UNIQUEUSE: {
        UM_CAST(UM_UNIQUEUSE, use, base);
        UM_DEFFIX(UM_UNIQUESTATUS, st);
        st->id = use->id;
        if (!unique_isuse(&self->uni, use->id)) {
            unique_use(&self->uni, use->id);
            st->status = UNIQUE_USE_OK;
        } else {
            st->status = UNIQUE_HAS_USED;
        }
        sh_service_send(SERVICE_ID, source, MT_UM, st, sizeof(*st));
        }
        break;
    case IDUM_UNIQUEUNUSE: {
        UM_CAST(UM_UNIQUEUNUSE, unuse, base);
        unique_unuse(&self->uni, unuse->id);
        } 
        break;
    }
}

void
uniqueol_main(struct service *s, int session, int source, int type, const void *msg, int n) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        process_unique(s, source, base);
        }
        break;
    case MT_MONITOR:
        // todo
        break;
    }
}
