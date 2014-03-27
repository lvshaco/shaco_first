#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"
#include "msg_client.h"

// status
#define S_ALLOC 0
#define S_AUTH_VERIFY 1
#define S_UNIQUE_VERIFY 2
#define S_UNIQUE_VERIFY_OK 3
#define S_HALL 4
#define S_MATCH 5
#define S_ROOM 6

// disconnect
#define DISCONNECT 1
#define NODISCONNECT 0

#define CONN_HASH(gate_source, connid) \
    (((uint64_t)(gate_source)) << 32 | (connid))

#define UID(ur) ((ur)->accid)

struct user {
    int gate_source;
    int connid;
    uint32_t wsession;
    uint32_t accid;
    int status;
    int auth_handle;
    int hall_handle;
    int room_handle;

    char account[ACCOUNT_NAME_MAX+1];
};
 
struct watchdog {
    int gate_handle;
    int auth_handle;
    int hall_handle;
    int room_handle; 
    int uniqueol_handle;
    bool is_uniqueol_ready;

    uint32_t wsession_alloctor;
    struct sh_hash64 conn2user;
    struct sh_hash acc2user;
};

// watchdog
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
    //sh_hash64_foreach(&self->conn2user, free);
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
        sh_monitor("gate", &h, &self->gate_handle) ||
        sh_monitor("auth", &h, &self->auth_handle) ||
        sh_monitor("hall", &h, &self->hall_handle) ||
        sh_monitor("room", &h, &self->room_handle)) {
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

static inline struct user *
alloc_user(struct watchdog *self, int gate_source, int connid) {
    struct user *ur = malloc(sizeof(*ur));
    ur->wsession = alloc_sessionid(self);
    ur->gate_source = gate_source;
    ur->connid = connid;
    ur->accid = 0;
    ur->status = S_ALLOC;
    ur->auth_handle = -1;
    ur->hall_handle = -1;
    ur->room_handle = -1;
    ur->account[0] = '\0';
    return ur;
}

static void
disconnect_client(struct module *s, int gate_source, int connid, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_LOGOUT, lo);
    g->connid = connid;
    lo->err = err;
    int sz = sizeof(*g)+sizeof(*lo);
    sh_module_send(MODULE_ID, gate_source, MT_UM, g, sz);
}

static void
logout(struct module *s, struct user *ur, int8_t err, int disconn) {
    struct watchdog *self = MODULE_SELF;

    uint64_t conn = CONN_HASH(ur->gate_source, ur->connid);

    sh_hash64_remove(&self->conn2user, conn);
    if (ur->accid > 0) {
        sh_hash_remove(&self->acc2user, ur->accid);
    }

    if (ur->status >= S_UNIQUE_VERIFY_OK) {
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
    if (disconn == DISCONNECT) {
        disconnect_client(s, ur->gate_source, ur->connid, err);
    } 
    free(ur);
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
            disconnect_client(s, source, connid, SERR_RELOGIN);
            return;
        }
        int auth_handle = sh_module_nextload(self->auth_handle);
        if (auth_handle == -1) {
            disconnect_client(s, source, connid, SERR_NOAUTHS);
            return;
        }
        ur = alloc_user(self, source, connid);
        memcpy(ur->account, la->account, sizeof(ur->account));
        assert(!sh_hash64_insert(&self->conn2user, conn, ur));
        ur->status = S_AUTH_VERIFY;
        ur->auth_handle = auth_handle;
        UM_DEFWRAP(UM_AUTH, w, UM_LOGINACCOUNT, wla);
        w->conn = conn;
        w->wsession = ur->wsession;
        *wla = *la;
        sh_module_send(MODULE_ID, auth_handle, MT_UM, w, sizeof(*w) + sizeof(*wla));
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
            UM_CAST(UM_NETDISCONN, disc, base);
            logout(s, ur, disc->err, NODISCONNECT);
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
    ur->auth_handle = -1;
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

    ur->status = S_UNIQUE_VERIFY_OK;
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
    if (ur->status != S_AUTH_VERIFY) {
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
    if (ur->status != S_UNIQUE_VERIFY) {
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
exit_hall(struct module *s, int source, uint32_t accid, int err) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur &&
        ur->hall_handle == source) {
        ur->hall_handle = -1;
        logout(s, ur, err, DISCONNECT);
    }
}

static inline void
login_room(struct module *s, struct UM_LOGINROOM *lr, int sz) {
    struct watchdog *self = MODULE_SELF;
    uint32_t accid  = lr->detail.accid;
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur &&
        ur->status == S_HALL) {
        ur->status = S_ROOM;
        ur->room_handle = lr->room_handle;
        sh_module_send(MODULE_ID, ur->room_handle, MT_UM, lr, sz);
    }
}

static inline void
notify_exit_room(struct module *s, struct user *ur, int err) {
    UM_DEFWRAP(UM_GATE, g, UM_GAMEEXIT, ge);
    g->connid = ur->connid;
    ge->err = err;
    sh_module_send(MODULE_ID, ur->gate_source, MT_UM, g, sizeof(*g) + sizeof(*ge));
}

static inline void
exit_room_directly(struct module *s, struct user *ur) {
    ur->status = S_HALL;
    ur->room_handle = -1;
    if (ur->hall_handle != -1) {
        UM_DEFFIX(UM_EXITROOM, exit);
        exit->uid = UID(ur);
        sh_module_send(MODULE_ID, ur->hall_handle, MT_UM, exit, sizeof(*exit));
    }
}

static inline void
exit_room(struct module *s, uint32_t uid) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, uid);
    if (ur == NULL)
        return;
    if (ur->status == S_ROOM) { 
        sh_trace("Watchdog user %u receive exit room", uid);
        exit_room_directly(s, ur);
    } else {
        sh_trace("Watchdog user %u receive exit room, but status %d", uid, ur->status);
    }
}

static inline void
over_room(struct module *s, uint32_t uid, int err) {
    struct watchdog *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->acc2user, uid);
    if (ur && 
        ur->status == S_ROOM) {
        ur->status = S_HALL;
        ur->room_handle = -1;
        notify_exit_room(s, ur, err);
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

static void
umsg(struct module *s, int source, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
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
    case IDUM_UNIQUEREADY: {
        self->is_uniqueol_ready = true;
        break;
        }
    case IDUM_EXITHALL: {
        UM_CAST(UM_EXITHALL, exit, msg);
        exit_hall(s, source, exit->uid, exit->err);
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
        exit_room(s, exit->uid);
        break;
        }
    case IDUM_OVERROOM: {
        UM_CAST(UM_OVERROOM, over, msg);
        over_room(s, over->uid, over->err);
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
}

// monitor exit
struct exitud {
    struct module *s;
    int source;
};

static void 
gate_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct module *s = eu->s;
    struct user *ur = pointer;
    if (ur->gate_source == eu->source) {
        logout(s, ur, SERR_GATEEXIT, NODISCONNECT);
    }
}

static void
hall_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct module *s = eu->s;
    struct user *ur = pointer;
    if (ur->hall_handle == eu->source) {
        ur->hall_handle = -1; // clear first
        logout(s, ur, SERR_HALLEXIT, DISCONNECT);
    }
}

static void
room_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct module *s = eu->s;
    struct user *ur = pointer;
    if (ur->status == S_ROOM &&
        ur->room_handle == eu->source) {
        notify_exit_room(s, ur, SERR_ROOMEXIT);
        exit_room_directly(s, ur);
    }
}

static void
auth_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct module *s = eu->s;
    struct user *ur = pointer;
    if (ur->status == S_AUTH_VERIFY &&
        ur->auth_handle == eu->source) {
        auth_fail(s, ur, SERR_AUTHEXIT);
    }
}

static void
uniqueol_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct module *s = eu->s;
    struct user *ur = pointer;
    if (ur->status == S_UNIQUE_VERIFY) {
        logout(s, ur, SERR_UNIQUEOLEXIT, DISCONNECT);
    }
}

static void
handle_exit(struct module *s, int source, void (*exitcb)(void*, void*)) {
    struct watchdog *self = MODULE_SELF;
    struct exitud ud = {s, source};
    sh_hash64_foreach2(&self->conn2user, exitcb, &ud);
}

static void
uniqueol_startcb(void *pointer, void *ud) {
    struct module *s = ud;
    struct watchdog *self = MODULE_SELF;
    struct user *ur = pointer;
    if (ur->status >= S_UNIQUE_VERIFY_OK) {
        UM_DEFFIX(UM_UNIQUEUSE, uni);
        uni->id = ur->accid;
        sh_module_send(MODULE_ID, self->uniqueol_handle, MT_UM, uni, sizeof(*uni));
    }
}

static void
uniqueol_start(struct module *s) {
    struct watchdog *self = MODULE_SELF;
    sh_hash_foreach2(&self->acc2user, uniqueol_startcb, s);
    UM_DEFFIX(UM_SYNCOK, ok);
    sh_module_send(MODULE_ID, self->uniqueol_handle, MT_UM, ok, sizeof(*ok));
}

static void
monitor(struct module *s, int source, const void *msg, int sz) {
    struct watchdog *self = MODULE_SELF;
    int type = sh_monitor_type(msg);
    int vhandle = sh_monitor_vhandle(msg);
    switch (type) {
    case MONITOR_START:
        if (vhandle == self->uniqueol_handle) {
            uniqueol_start(s);
        }
        break;
    case MONITOR_EXIT:
        if (vhandle == self->uniqueol_handle) {
            self->is_uniqueol_ready = false;
            handle_exit(s, source, uniqueol_exitcb);
        } else if (vhandle == self->gate_handle) {
            handle_exit(s, source, gate_exitcb);
        } else if (vhandle == self->hall_handle) {
            handle_exit(s, source, hall_exitcb);
        } else if (vhandle == self->room_handle) {
            handle_exit(s, source, room_exitcb);
        } else if (vhandle == self->auth_handle) {
            handle_exit(s, source, auth_exitcb);
        }
        break;
    }
}

static int
nuser(struct module *s, struct args *A, struct memrw *rw) {
    struct watchdog *self = MODULE_SELF;
    uint32_t ntotal = self->conn2user.used;
    uint32_t nverifyed = self->acc2user.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(verifyed) %u(all)", nverifyed, ntotal);
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
user(struct module *s, struct args *A, struct memrw *rw) {
    struct watchdog *self = MODULE_SELF;
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    int n;
    uint32_t accid = strtoul(A->argv[1], NULL, 10);
    struct user *ur = sh_hash_find(&self->acc2user, accid);
    if (ur) {
        n = snprintf(rw->ptr, RW_SPACE(rw), 
                    "accid(%u) acc(%s) status(%d) connid(%d) wsession(%u) "
                    "gate(%04x) auth(%04x) hall(%04x) room(%04x)", 
                    ur->accid, ur->account, ur->status, ur->connid, ur->wsession,
                    ur->gate_source, ur->auth_handle, ur->hall_handle, ur->room_handle);
    } else {
        n = snprintf(rw->ptr, RW_SPACE(rw), "none");
    }
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    //struct watchdog *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "nuser")) {
        return nuser(s, &A, rw);
    } else if (!strcmp(cmd, "user")) {
        return user(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

void
watchdog_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
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
