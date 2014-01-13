#include "sc_service.h"
#include "sc_node.h"
#include "user_message.h"
#include <string.h>
#include <stdlib.h>

#define S_AUTH_VERIFY 1
#define S_UNIQUE_VERIFY 2
#define S_HALL 3
#define S_MATCH 4
#define S_ROOM 5

struct user {
    uint32_t connid;
    uint32_t accid;
    int status;
};
 
struct watchdog {
    int auth_handle;
    int hall_handle;
    int room_handle; 
    int uniqueol_handle;
    bool is_uniqueol_ready;

    struct sh_hash conn2user;
    struct sh_hash acc2user;
};

struct watchdog *
watchdog_create() {
    struct watchdog *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
watchdog_free(struct watchdog *self) {
    if (self == NULL)
        return;
    free(self);
}

int
watchdog_init(struct service *s) {
    struct watchdog *self = SERVICE_SELF;

    if (sc_handler("auth", &self->auth_handle) ||
        sc_handler("hall", &self->hall_handle) ||
        sc_handler("room", &self->room_handle)) {
        return 1;
    }
    return 0;
}

void
watchdog_time(struct service *s) {
    //struct watchdog *self = SERVICE_SELF;
}

static inline void
response_loginfail(struct service *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGINACCOUNTFAIL);
    g->connid = connid;
    UM_CAST(UM_LOGINACCOUNTFAIL, fail, g->wrap);
    fail->err = err;
    sh_service_send(SERVICE_ID, gate_source, MT_UM, g, sizeof(*g) + sizeof(*fail));
}

static void
auth(struct service *s, int source, int connid, ) {
    int auth = sc_service_nextload(self->auth_handle);
    if (auth == -1) {
        response_loginfail(s, source, connid, SERR_NOAUTHS);
        return;
    }
}

static inline struct user *
alloc_user(uint32_t conn) {
    struct user *ur = malloc(sizeof(*ur));
    memset(ur, 0, sizeof(*ur));
    ur->conn = conn;
    return ur;
}

static void
process_gate(struct service *s, int source, int connid, const void *msg, int sz) {
    struct watchdog *self = SERVICE_SELF;
    
    uint32_t conn = source << 16 | connid; 

    UM_CAST(UM_BASE, base, msg); 

    if (base->msgid == IDUM_LOGINACCOUNT) {
        UM_CAST(UM_LOGINACCOUNT, la, base);
        struct user *ur = sh_hash_find(&self->conn2user, conn);
        if (ur) {
            response_loginfail(s, source, connid, SERR_RELOGIN);
            return;
        }
        int auth = sc_service_nextload(self->auth_handle);
        if (auth == -1) {
            response_loginfail(s, source, connid, SERR_NOAUTHS);
            return;
        }
        ur = allco_user(conn);
        ur->status = S_AUTH_VERIFY;

        UM_DEFWRAP(UM_WATCHDOG, w, UM_LOGINACCOUNT);
        UM_CAST(UM_LOGINACCOUNT, wla, w->wrap);
        *wla = *la;
        sh_service_send(SERVICE_ID, auth, MT_UM, w, sizeof(*w) + sizeof(*wla));
    } else {
        struct user *ur = sh_hash_find(&self->conn2user, conn);
        if (ur == NULL) {
            response_loginfail(s, source, connid, SERR_NOLOGIN);
            return;
        }
        switch (base->msgid) {
        case IDUM_LOGOUT:
            break;
        } 
    } 
}

static void
process_watchdog(struct service *s, int source, uint32_t conn, const void *msg) {

    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_LOGINACCOUNTFAIL:
        // todo
        break;
    }
}

void
watchdog_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg); 
        switch (base->msgid) {
        case IDUM_GATE: {
            UM_CAST(UM_GATE, g, msg);
            process_gate(s, source, g->connid, g->wrap, sz-sizeof(*g));
            }
            break;
        case IDUM_WATCHDOG: {
            UM_CAST(UM_WATCHDOG, w, msg);
            process_watchdog(s, source, w->conn, w->wrap);
            }
            break;
        }
        }
        break;
    case MT_MONITOR:
        // todo
        break;
    }
}
