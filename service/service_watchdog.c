#include "sc_service.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sh_hash.h"
#include "user_message.h"
#include "cli_message.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// status
#define S_ALLOC 0
#define S_AUTH_VERIFY 1
#define S_UNIQUE_VERIFY 2
#define S_HALL 3
#define S_MATCH 4
#define S_ROOM 5

// disconnect
#define DISCONNECT 1
#define NODISCONNECT 0

#define CONN_HASH(gate_source, connid) \
    (((uint64_t)(gate_source)) << 32 | (connid))

struct user {
    int gate_source;
    int connid;
    uint32_t wsession;
    uint32_t accid;
    int status;
    int hall_handle;
    int room_handle;
};
 
struct watchdog {
    int auth_handle;
    int hall_handle;
    int room_handle; 
    int uniqueol_handle;
    bool is_uniqueol_ready;

    uint32_t wsession_alloctor;
    struct sh_hash64 conn2user;
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

static uint32_t 
alloc_sessionid(struct watchdog *self) {
    uint32_t id = ++self->wsession_alloctor;
    if (id == 0)
        id = ++self->wsession_alloctor;
    return id;
}

static inline void
response_loginfail(struct service *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGINACCOUNTFAIL);
    g->connid = connid;
    UM_CAST(UM_LOGINACCOUNTFAIL, fail, g->wrap);
    fail->err = err;
    sh_service_send(SERVICE_ID, gate_source, MT_UM, g, sizeof(*g) + sizeof(*fail));
}

static inline struct user *
alloc_user(struct watchdog *self, int gate_source, int connid) {
    struct user *ur = malloc(sizeof(*ur));
    ur->wsession = alloc_sessionid(self);
    ur->gate_source = gate_source;
    ur->connid = connid;
    ur->accid = 0;
    ur->status = S_ALLOC;
    ur->hall_handle = -1;
    ur->room_handle = -1;
    return ur;
}

static inline void
free_user(struct user *ur) {
    free(ur);
}

static void
disconnect_client(struct service *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGOUT);
    g->connid = connid;
    UM_CAST(UM_LOGOUT, lo, g->wrap);
    lo->err = err;
    int sz = sizeof(*g)+sizeof(*lo);
    sh_service_send(SERVICE_ID, gate_source, MT_UM, g, sz);
}

static void
logout(struct service *s, struct user *ur, int8_t err, int disconn) {
    struct watchdog *self = SERVICE_SELF;

    uint64_t conn = CONN_HASH(ur->gate_source, ur->connid);

    sh_hash64_remove(&self->conn2user, conn);
    if (ur->accid > 0) {
        sh_hash_remove(&self->acc2user, ur->accid);
    }

    if (ur->status > S_UNIQUE_VERIFY) {
        UM_DEFFIX(UM_UNIQUEUNUSE, uni);
        uni->id = ur->accid;
        sh_service_send(SERVICE_ID, self->uniqueol_handle, MT_UM, uni, sizeof(*uni));
    }
    if (ur->hall_handle != -1) {
        UM_DEFWRAP(UM_HALL, ha, UM_LOGOUT);
        ha->accid = ur->accid;
        UM_CAST(UM_LOGOUT, lo, ha->wrap);
        lo->err = err;
        int sz = sizeof(*ha) + sizeof(*lo);
        sh_service_send(SERVICE_ID, ur->hall_handle, MT_UM, ha, sz);
    }
    if (ur->room_handle != -1) {
        UM_DEFWRAP(UM_ROOM, ro, UM_LOGOUT);
        ro->accid = ur->accid;
        UM_CAST(UM_LOGOUT, lo, ro->wrap);
        lo->err = err;
        int sz = sizeof(*ro) + sizeof(*lo);
        sh_service_send(SERVICE_ID, ur->room_handle, MT_UM, ro, sz);
    } 
    if (disconn == DISCONNECT) {
        disconnect_client(s, ur->gate_source, ur->connid, err);
    } 
    free_user(ur);
}

static void
process_gate(struct service *s, int source, int connid, const void *msg, int sz) {
    struct watchdog *self = SERVICE_SELF;
    
    uint64_t conn = CONN_HASH(source, connid);

    UM_CAST(UM_BASE, base, msg); 

    if (base->msgid == IDUM_LOGINACCOUNT) {
        UM_CAST(UM_LOGINACCOUNT, la, base);
        struct user *ur = sh_hash64_find(&self->conn2user, conn);
        if (ur) {
            disconnect_client(s, source, connid, SERR_RELOGIN);
            return;
        }
        int auth = sc_service_nextload(self->auth_handle);
        if (auth == -1) {
            disconnect_client(s, source, connid, SERR_NOAUTHS);
            return;
        }
        ur = alloc_user(self, source, connid);
        assert(!sh_hash64_insert(&self->conn2user, conn, ur));
        ur->status = S_AUTH_VERIFY;
        
        UM_DEFWRAP(UM_AUTH, w, UM_LOGINACCOUNT);
        w->conn = conn;
        w->wsession = ur->wsession;
        UM_CAST(UM_LOGINACCOUNT, wla, w->wrap);
        *wla = *la;
        sh_service_send(SERVICE_ID, auth, MT_UM, w, sizeof(*w) + sizeof(*wla));
        return;
    } 
    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur == NULL) {
        disconnect_client(s, source, connid, SERR_NOLOGIN);
        return;
    }
    if (base->msgid >= IDUM_HALLB && base->msgid <= IDUM_HALLE) {
        if (ur->status == S_HALL && ur->hall_handle != -1) {
            UM_DEFWRAP2(UM_HALL, ha, sz);
            ha->accid = ur->accid;
            memcpy(ha->wrap, msg, sz);
            sh_service_send(SERVICE_ID, ur->hall_handle, MT_UM, ha, sizeof(*ha)+sz);
        }
    } else if (base->msgid >= IDUM_ROOMB && base->msgid <= IDUM_ROOME) {
        if (ur->status == S_ROOM && ur->room_handle != -1) {
            UM_DEFWRAP2(UM_ROOM, ro, sz);
            ro->accid = ur->accid;
            memcpy(ro->wrap, msg, sz);
            sh_service_send(SERVICE_ID, ur->room_handle, MT_UM, ro, sizeof(*ro)+sz);
        }
    } else {
        switch (base->msgid) {
        case IDUM_NETDISCONN: {
            UM_CAST(UM_NETDISCONN, disc, base);
            logout(s, ur, disc->err, NODISCONNECT);
            break;
            }
        }
    }
}

static void
auth_ok(struct service *s, struct user *ur, uint32_t accid) {
    struct watchdog *self = SERVICE_SELF;

    if (!self->is_uniqueol_ready) {
        logout(s, ur, SERR_NOUNIQUEOL, DISCONNECT);
        return;
    }
    if (sh_hash_insert(&self->acc2user, accid, ur)) {
        logout(s, ur, SERR_ACCINSERT, DISCONNECT);
        return;
    }
    ur->accid = accid;
    ur->status = S_UNIQUE_VERIFY;

    UM_DEFFIX(UM_UNIQUEUSE, uni);
    uni->id = ur->accid;
    sh_service_send(SERVICE_ID, self->uniqueol_handle, MT_UM, uni, sizeof(*uni));
}

static void
auth_fail(struct service *s, struct user *ur, int8_t err) {
    logout(s, ur, err, DISCONNECT);
}

static void
uniqueol_ok(struct service *s, struct user *ur) {
    struct watchdog *self = SERVICE_SELF;

    int hall_handle = sc_service_minload(self->hall_handle);
    if (hall_handle == -1) {
        logout(s, ur, SERR_NOHALLS, DISCONNECT);
        return;
    }
    ur->status = S_HALL;
    ur->hall_handle = hall_handle;

    UM_DEFFIX(UM_ENTERHALL, enter);
    enter->accid = ur->accid;
    sh_service_send(SERVICE_ID, hall_handle, MT_UM, enter, sizeof(*enter));
}

static void
uniqueol_fail(struct service *s, struct user *ur) {
    logout(s, ur, SERR_ACCLOGINED, DISCONNECT);
}

static void
process_auth(struct service *s, int source, uint64_t conn, uint32_t wsession, const void *msg) {
    struct watchdog *self = SERVICE_SELF;

    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur == NULL) {
        return;
    }
    if (ur->wsession != wsession) {
        return;
    }
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_LOGINACCOUNTOK: {
        UM_CAST(UM_LOGINACCOUNTOK, ok, base);
        auth_ok(s, ur, ok->accid);
        break;
        }
    case IDUM_LOGINACCOUNTFAIL: {
        UM_CAST(UM_LOGINACCOUNTFAIL, fail, base);
        auth_fail(s, ur, fail->err);
        break;
        }
    }
}

static void
process_unique(struct service *s, uint32_t id, int status) {
    struct watchdog *self = SERVICE_SELF;

    struct user *ur = sh_hash_find(&self->acc2user, id);
    if (ur == NULL) {
        return;
    }
    switch (status) {
    case UNIQUE_USE_OK:
        uniqueol_ok(s, ur);
        break;
    case UNIQUE_HAS_USED:
        uniqueol_fail(s, ur);
        break;
    }
}

static void
process_role(struct service *s, uint32_t accid, const void *msg, int sz) {
    struct watchdog *self = SERVICE_SELF;

    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur == NULL) {
        return;
    }
    if (sz < sizeof(struct UM_BASE)) {
        sc_error("Invalid role message, sz %d", sz);
        return;
    } 
    if (sz + sizeof(struct UM_GATE) > UM_MAXSZ) {
        UM_CAST(UM_BASE, base, msg);
        sc_error("Too large role message %d", base->msgid);
        return;
    }
    UM_DEFWRAP2(UM_GATE, g, sz);
    g->connid = ur->connid;
    memcpy(g->wrap, msg, sz);
    sh_service_send(SERVICE_ID, ur->gate_source, MT_UM, g, sizeof(*g) + sz);
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
            break;
            }
        case IDUM_AUTH: {
            UM_CAST(UM_AUTH, w, msg);
            process_auth(s, source, w->conn, w->wsession, w->wrap);
            break;
            }
        case IDUM_UNIQUESTATUS: {
            UM_CAST(UM_UNIQUESTATUS, u, msg);
            process_unique(s, u->id, u->status);
            break;
            }
        case IDUM_HALL: {
            UM_CAST(UM_HALL, ha, msg);
            process_role(s, ha->accid, ha->wrap, sz-sizeof(*ha));
            break;
            }
        case IDUM_ROOM: {
            UM_CAST(UM_ROOM, ro, msg);
            process_role(s, ro->accid, ro->wrap, sz-sizeof(*ro));
            break;
            }
        }
        break;
        }
    case MT_MONITOR:
        // todo
        break;
    }
}
