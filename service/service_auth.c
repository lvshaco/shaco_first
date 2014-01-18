#include "sc_service.h"
#include <stdlib.h>

/*#define STATE_FREE   0*/
//#define STATE_LOGIN     1
//#define STATE_VERIFYED  2
//#define STATE_GATELOAD  3
//#define STATE_DONE      4

struct user {
    uint64_t conn;
    int watchdog_source;
    uint32_t wsession;
    //int state;  // see STATE_*
    //uint64_t login_time;
    //uint32_t accid;
    //uint64_t key;
    char account[ACCOUNT_NAME_MAX];
    char passwd[ACCOUNT_PASSWD_MAX];
};


struct auth {
    int watchdog_handle;
    int rpacc_handle;
    struct sh_hash64 conn2user;
    struct redis_reply reply;
};

struct auth *
auth_create() {
    struct auth *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
auth_free(struct auth *self) {
    if (self == NULL)
        return;
    redis_finireply(&self->reply);
    sh_hash64_foreach(&self->reply, free);
    sh_hash64_fini(&self->reply);
    free(self);
}

int
auth_init(struct service *s) {
    //struct auth *self = SERVICE_SELF;
    return 0;
}

static void
login(struct service *s, int source, uint64_t conn, uint32_t wsession, struct UM_LOGINACCOUNT *la) {
    struct auth *self = SERVICE_SELF; 
    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur) {
        return;
    }
    ur = malloc(sizeof(*ur));
    ur->conn = conn;
    ur->watchdog_source = source;
    ur->wsession = wsession;
    memcpy(ur->account, la->account, sizeof(ur->account));
    memcpy(ur->passwd, la->passwd, sizeof(ur->passwd));
    assert(!sh_hash64_insert(&self->conn2user, conn, ur));

    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->needreply = 1;
    rq->needrecord = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &ur->conn, sizeof(ur->conn));
    memrw_write(&rw, &ur->wsession, sizeof(ur->wsession));
    rq->cbsz = RW_CUR(&rw);
    int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmget acc:%s id passwd\r\n", p->account);
    memrw_pos(&rw, len);
    int msgsz = sizeof(*rq) + RW_CUR(&rw);
    sh_service_send(SERVICE_ID, self->rpacc_handle, MT_UM, rq, msgsz);
}

static inline void
notify_login_fail(struct service *s, struct user *ur, int err) {
    UM_DEFWRAP(UM_AUTH, au, UM_LOGINACCOUNTFAIL, fail);
    au->conn = ur->conn;
    au->wsession = ur->wsession;
    fail->err = err;
    sh_service_send(SERVICE_ID, ur->watchdog_source, MT_UM, au, sizeof(*au)+sizeof(*fail));
}

static inline void
notify_login_ok(struct service *s, struct user *ur, uint32_t accid) {
    UM_DEFWRAP(UM_AUTH, au, UM_LOGINACCOUNTOK, ok);
    au->conn = ur->conn;
    au->wsession = ur->wsession;
    ok->accid = accid;
    sh_service_send(SERVICE_ID, ur->watchdog_source, MT_UM, au, sizeof(*au)+sizeof(*ok));
}

static void
process_redis(struct service *s, struct UM_REDISREPLY *rep, int sz) {
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));
    uint64_t conn = 0;
    memrw_read(&rw, &conn, sizeof(conn));
    uint32_t wsession = 0;
    memrw_read(&rw, &wsession, sizeof(wsession));
    
    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur == NULL || ur->wsession != wsession) {
        return;
    }
    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
        notify_login_fail(s, ur, SERR_DBREPLY);
        return;
    }
    struct redis_replyitem* item = self->reply.stack[0];
    if (item->type != REDIS_REPLY_ARRAY) {
        notify_login_fail(s, ur, SERR_DBREPLYTYPE);
        return;
    }
    int n = item->value.i;
    if (n != 2) {
        notify_login_fail(s, ur, SERR_DBERR);
        return;
    }
    uint32_t accid = redis_bulkitem_toul(&item->child[0]);
    if (accid == 0) {
        notify_login_fail(s, ur, SERR_NOACC);
        return;
    }
    if (strncmp(ur->passwd, item->child[1].value.p, item->child[1].value.len)) {
        notify_login_fail(s, ur, SERR_ACCVERIFY);
        return;
    }
    notify_login_ok(s, ur, accid);
    sh_hash64_remove(self->conn2user, conn);
    free(ur);
}

void
auth_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_AUTH: {
            UM_CAST(UM_AUTH, au, msg);
            UM_CAST(UM_BASE, wrap, au->wrap);
            switch (wrap->msgid) {
            case IDUM_LOGINACCOUNT: {
                UM_CASTCK(UM_LOGINACCOUNT, la, wrap, sz-sizeof(*au));
                login(s, source, au->conn, au->wsession, la);
                break;
                }
            }
            break;
            }
        case IDUM_REDISREPLY: {
            UM_CAST(UM_REDISREPLY, rep, msg);
            process_redis(s, rep, sz);
            break;
            }
        }
        break;
        }
    }
}

void
auth_time(struct service *s) {
    //struct auth *self = SERVICE_SELF;
}
