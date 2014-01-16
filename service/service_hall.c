#include "sc_service.h"
#include "sc_node.h"
#include <stdlib.h>

struct player {
    int watchdog_source;
    int status;
    int createchar_times;
    int roomid;
    int cu_flag; // see CU_GRADE
    struct chardata data;
};

struct hall {
    int match_handle;
    int watchdog_handle;
};

struct hall *
hall_create() {
    struct hall *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return hall;
}

void
hall_free(struct hall *self) {
    if (self == NULL)
        return;
    free(self);
}

int
hall_init(struct service *s) {
    struct hall *self = SERVICE_SELF;

    if (sh_handler("watchdog", &self->watchdog_handle) ||
        sh_handler("match", &self->match_handle)) {
        return 1;
    }
    return 0;
}

void
hall_time(struct service *s) {
    //struct hall *self = SERVICE_SELF;
}

static void
login(struct service *s, int source, uint32_t accid) {
}

static void
logout(struct service *s, uint32_t accid) {
}

static void
create_char(struct service *s, uint32_t accid, char name[CHAR_NAME_MAX]) {
}

void
hall_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    struct hall *self = SERVICE_SELF;

    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ENTERHALL: {
            UM_CAST(UM_ENTERHALL, eh, msg);
            login(s, source, eh->accid);
            break;
            }
        case IDUM_HALL: {
            // todo
            UM_CAST(UM_HALL, ha, msg);
            UM_CAST(UM_BASE, wrap, ha->wrap);
            switch (wrap->msgid) {
            case IDUM_LOGOUT:
                logout(s, ha->charid);
                break;
            case IDUM_CHARCREATE: {
                UM_CAST(UM_CHARCREATE, create, wrap);
                create_char(s, ha->charid, create->name);
                break;
                }
            }
            break;
            }
        }
        break;
        }
    case MT_MONITOR:
        break;
    }
}
