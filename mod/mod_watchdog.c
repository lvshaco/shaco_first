#include "sh.h"
#include "msg_server.h"
#include "msg_client.h"

// status
#define S_ALLOC 0
#define S_AUTH_VERIFY 1
#define S_UNIQUE_VERIFY 2
#define S_HALL 3
#define S_MATCH 4
#define S_ROOM 5

// dishonnect
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
    sh_hash64_fini(&self->conn2user);
    sh_hash_fini(&self->acc2user);
    free(self);
}

int
watchdog_init(struct module *s) {
    struct watchdog *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER|PUB_MOD)) {
        return 1;
    }
    struct sh_monitor_handle h = {MODULE_ID, MODULE_ID};
    if (sh_monitor("uniqueol", &h, &self->uniqueol_handle) ||
        sh_handler("gate", SUB_REMOTE, &self->auth_handle) ||
        sh_handler("auth", SUB_REMOTE, &self->auth_handle) ||
        sh_handler("hall", SUB_REMOTE, &self->hall_handle) ||
        sh_handler("room", SUB_REMOTE, &self->room_handle)) {
        return 1;
    }
    sh_hash64_init(&self->conn2user, 1);
    sh_hash_init(&self->acc2user, 1);
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
response_loginfail(struct module *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGINACCOUNTFAIL, fail);
    g->connid = connid;
    fail->err = err;
    sh_module_send(MODULE_ID, gate_source, MT_UM, g, sizeof(*g) + sizeof(*fail));
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
dishonnect_client(struct module *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGOUT, lo);
    g->connid = connid;
    lo->err = err;
    int sz = sizeof(*g)+sizeof(*lo);
    sh_module_send(MODULE_ID, gate_source, MT_UM, g, sz);
}

static void
logout(struct module *s, struct user *ur, int8_t err, int dishonn) {
    struct watchdog *self = MODULE_SELF;

    uint64_t conn = CONN_HASH(ur->gate_source, ur->connid);

    sh_hash64_remove(&self->conn2user, conn);
    if (ur->accid > 0) {
        sh_hash_remove(&self->acc2user, ur->accid);
    }

    if (ur->status > S_UNIQUE_VERIFY) {
        UM_DEFFIX(UM_UNIQUEUNUSE, uni);
        uni->id = ur->accid;
        sh_module_send(MODULE_ID, self->uniqueol_handle, MT_UM, uni, sizeof(*uni));
    }
    if (ur->hall_handle != -1) {
        UM_DEFWRAP(UM_HALL, ha, UM_LOGOUT, lo);
        ha->uid = ur->accid;
        lo->err = err;
        int sz = sizeof(*ha) + sizeof(*lo);
        sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, ha, sz);
    }
    if (ur->room_handle != -1) {
        UM_DEFWRAP(UM_ROOM, ro, UM_LOGOUT, lo);
        ro->uid = ur->accid;
        lo->err = err;
        int sz = sizeof(*ro) + sizeof(*lo);
        sh_module_send(MODULE_ID, ur->room_handle, MT_UM, ro, sz);
    } 
    if (dishonn == DISCONNECT) {
        dishonnect_client(s, ur->gate_source, ur->connid, err);
    } 
    free_user(ur);
}

static void
process_gate(struct module *s, int source, int connid, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
    
    uint64_t conn = CONN_HASH(source, connid);

    UM_CAST(UM_BASE, base, msg); 
    
    if (base->msgid == IDUM_LOGINACCOUNT) {
        UM_CASTCK(UM_LOGINACCOUNT, la, base, sz);
        struct user *ur = sh_hash64_find(&self->conn2user, conn);
        if (ur) {
            dishonnect_client(s, source, connid, SERR_RELOGIN);
            return;
        }
        int auth = sh_module_nextload(self->auth_handle);
        if (auth == -1) {
            dishonnect_client(s, source, connid, SERR_NOAUTHS);
            return;
        }
        ur = alloc_user(self, source, connid);
        assert(!sh_hash64_insert(&self->conn2user, conn, ur));
        ur->status = S_AUTH_VERIFY;

        UM_DEFWRAP(UM_AUTH, w, UM_LOGINACCOUNT, wla);
        w->conn = conn;
        w->wsession = ur->wsession;
        *wla = *la;
        sh_module_send(MODULE_ID, auth, MT_UM, w, sizeof(*w) + sizeof(*wla));
        return;
    } 
    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur == NULL) {
        dishonnect_client(s, source, connid, SERR_NOLOGIN);
        return;
    }
    if (base->msgid >= IDUM_HALLB && base->msgid <= IDUM_HALLE) {
        if (ur->status == S_HALL && ur->hall_handle != -1) {
            UM_DEFWRAP2(UM_HALL, ha, sz);
            ha->uid = ur->accid;
            memcpy(ha->wrap, msg, sz);
            sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, ha, sizeof(*ha)+sz);
        }
    } else if (base->msgid >= IDUM_ROOMB && base->msgid <= IDUM_ROOME) {
        if (ur->status == S_ROOM && ur->room_handle != -1) {
            UM_DEFWRAP2(UM_ROOM, ro, sz);
            ro->uid = ur->accid;
            memcpy(ro->wrap, msg, sz);
            sh_module_send(MODULE_ID, ur->room_handle, MT_UM, ro, sizeof(*ro)+sz);
        }
    } else {
        switch (base->msgid) {
        case IDUM_NETDISCONN: {
            UM_CAST(UM_NETDISCONN, dish, base);
            logout(s, ur, dish->err, NODISCONNECT);
            break;
            }
        }
    }
}

static void
auth_ok(struct module *s, struct user *ur, uint32_t accid) {
    struct watchdog *self = MODULE_SELF;

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
    sh_module_send(MODULE_ID, self->uniqueol_handle, MT_UM, uni, sizeof(*uni));
}

static void
auth_fail(struct module *s, struct user *ur, int8_t err) {
    logout(s, ur, err, DISCONNECT);
}

static void
uniqueol_ok(struct module *s, struct user *ur) {
    struct watchdog *self = MODULE_SELF;

    int hall_handle = sh_module_minload(self->hall_handle);
    if (hall_handle == -1) {
        logout(s, ur, SERR_NOHALLS, DISCONNECT);
        return;
    }
    ur->status = S_HALL;
    ur->hall_handle = hall_handle;

    UM_DEFFIX(UM_ENTERHALL, enter);
    enter->uid = ur->accid;
    sh_module_send(MODULE_ID, hall_handle, MT_UM, enter, sizeof(*enter));
}

static void
uniqueol_fail(struct module *s, struct user *ur) {
    logout(s, ur, SERR_ACCLOGINED, DISCONNECT);
}

static inline void
process_auth(struct module *s, int source, uint64_t conn, uint32_t wsession, const void *msg) {
    struct watchdog *self = MODULE_SELF;

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

static inline void
process_unique(struct module *s, uint32_t id, int status) {
    struct watchdog *self = MODULE_SELF;

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

static inline void
process_hall(struct module *s, uint32_t accid, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur) {
        sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, msg, sz);
    }
}

static inline void
process_room(struct module *s, uint32_t accid, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur) {
        sh_module_send(MODULE_ID, ur->room_handle, MT_UM, msg, sz);
    }
}

static inline void
exit_hall(struct module *s, uint32_t accid, int err) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur) {
        ur->hall_handle = -1;
        logout(s, ur, err, DISCONNECT);
    }
}

static inline void
login_room(struct module *s, struct UM_LOGINROOM *lr, int sz) {
    struct watchdog *self = MODULE_SELF;
    uint32_t accid  = lr->detail.accid;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur == NULL) {
        return;
    }
    if (sh_module_has(self->room_handle, lr->room_handle)) {
        ur->status = S_ROOM;
        ur->room_handle = lr->room_handle;
        sh_module_send(MODULE_ID, ur->room_handle, MT_UM, lr, sz);
    } else {
        // todo notify hall exit room status (now match module do this)
        //sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, msg, sz);
    }
}

static inline void
exit_room(struct module *s, struct UM_EXITROOM *exit, int sz) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, exit->uid);
    if (ur) {
        ur->status = S_HALL;
        ur->room_handle = -1;
        if (ur->hall_handle != -1) {
            sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, exit, sz);
        }
    }
}

static inline void
process_client(struct module *s, uint32_t accid, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur == NULL) {
        return;
    }
    if (sz < sizeof(struct UM_BASE)) {
        sh_error("Invalid role message, sz %d", sz);
        return;
    } 
    if (sz + sizeof(struct UM_GATE) > UM_MAXSZ) {
        UM_CAST(UM_BASE, base, msg);
        sh_error("Too large role message %d", base->msgid);
        return;
    }
    UM_DEFWRAP2(UM_GATE, g, sz);
    g->connid = ur->connid;
    memcpy(g->wrap, msg, sz);
    sh_module_send(MODULE_ID, ur->gate_source, MT_UM, g, sizeof(*g) + sz);
}

void
watchdog_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
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
        case IDUM_EXITHALL: {
            UM_CAST(UM_EXITHALL, exit, msg);
            exit_hall(s, exit->uid, exit->err);
            break;
            }
        case IDUM_HALL: {
            UM_CAST(UM_HALL, ha, msg);
            process_hall(s, ha->uid, msg, sz);
            break;
            }
        case IDUM_LOGINROOM: {
            UM_CAST(UM_LOGINROOM, lr, msg);
            login_room(s, lr, sz);
            break;
            }
        case IDUM_EXITROOM: {
            UM_CAST(UM_EXITROOM, exit, msg);
            exit_room(s, exit, sz);
            break;
            }
        case IDUM_ROOM: {
            UM_CAST(UM_ROOM, ro, msg);
            process_room(s, ro->uid, msg, sz);
            break;
            }
        case IDUM_CLIENT: {
            UM_CAST(UM_CLIENT, cl, msg);
            process_client(s, cl->uid, cl->wrap, sz-sizeof(*cl));
            break;
            }
        }
        break;
        }
    case MT_MONITOR: {
        int type = ((uint8_t*)msg)[0];
        int vhandle = sh_from_littleendian32(msg+1);
        if (vhandle == self->uniqueol_handle) {
            // todo
            switch (type) {
            case MONITOR_START:
                self->is_uniqueol_ready = true;
                break;
            case MONITOR_EXIT:
                self->is_uniqueol_ready = false;
                break;
            }
        }
        break;
        }
    }
}
