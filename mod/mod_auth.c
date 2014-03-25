#include "sh.h"
#include "sh_hash.h"
#include "cmdctl.h"
#include "redis.h"
#include "msg_client.h"
#include "msg_server.h"
#include "memrw.h"

static const char *PASSWD = "7c4a8d09ca3762af61e59520943dc26494f8941b";
#define T_QUERY 0
#define T_GETID 1
#define T_SETID 2
#define T_SETPASSWD 3

struct user {
    uint64_t conn;
    int watchdog_source;
    uint32_t wsession;
    //uint64_t logintime;
    uint32_t accid;
    char account[ACCOUNT_NAME_MAX+1];
    char passwd[ACCOUNT_PASSWD_MAX+1];
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
    //sh_hash64_foreach(&self->conn2user, free);
    sh_hash64_fini(&self->conn2user);
    free(self);
}

int
auth_init(struct module *s) {
    struct auth *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("watchdog", SUB_REMOTE, &self->watchdog_handle) ||
        sh_handler("rpacc", SUB_LOCAL|SUB_REMOTE, &self->rpacc_handle)) {
        return 1;
    }
    redis_initreply(&self->reply, 512, 0);
    sh_hash64_init(&self->conn2user, 1);
    return 0;
}

static void
db(struct module *s, struct user *ur, int8_t type, const char *cmd, int len) {
    assert(len);
    struct auth *self = MODULE_SELF;
    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->flag = RQUERY_REPLY;
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &ur->conn, sizeof(ur->conn));
    //memrw_write(&rw, &ur->wsession, sizeof(ur->wsession));
    memrw_write(&rw, ur->account, sizeof(ur->account));
    memrw_write(&rw, &type, sizeof(type));
    rq->cbsz = RW_CUR(&rw);

    memcpy(rw.ptr, cmd, len);
    memrw_pos(&rw, len);

    int msgsz = sizeof(*rq) + RW_CUR(&rw);
    sh_module_send(MODULE_ID, self->rpacc_handle, MT_UM, rq, msgsz);
}

static void
login(struct module *s, int source, uint64_t conn, uint32_t wsession, struct UM_LOGINACCOUNT *la) {
    struct auth *self = MODULE_SELF; 
    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur) {
        // if db no reply, then user will no remove, we just reuse it as cache
    } else {
        ur = malloc(sizeof(*ur)); 
        assert(!sh_hash64_insert(&self->conn2user, conn, ur));
    }
    ur->conn = conn;
    ur->watchdog_source = source;
    ur->wsession = wsession;
    //ur->logintime = sh_timer_now();
    memcpy(ur->account, la->account, sizeof(ur->account));
    memcpy(ur->passwd, la->passwd, sizeof(ur->passwd));
  
    char cmd[1024], *tmp = cmd;
    int len = redis_format(&tmp, sizeof(cmd), "hmget acc:%s id passwd", ur->account);
    db(s, ur, T_QUERY, cmd, len);
}

static inline void
notify_login_fail(struct module *s, struct user *ur, int err) {
    UM_DEFWRAP(UM_AUTH, au, UM_LOGINACCOUNTFAIL, fail);
    au->conn = ur->conn;
    au->wsession = ur->wsession;
    fail->err = err;
    sh_module_send(MODULE_ID, ur->watchdog_source, MT_UM, au, sizeof(*au)+sizeof(*fail));
}

static inline void
notify_login_ok(struct module *s, struct user *ur, uint32_t accid) {
    UM_DEFWRAP(UM_AUTH, au, UM_LOGINACCOUNTOK, ok);
    au->conn = ur->conn;
    au->wsession = ur->wsession;
    ok->accid = accid;
    sh_module_send(MODULE_ID, ur->watchdog_source, MT_UM, au, sizeof(*au)+sizeof(*ok));
}

static void
process_redis(struct module *s, struct UM_REDISREPLY *rep, int sz) {
    struct auth *self = MODULE_SELF;
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));
    uint64_t conn = 0;
    memrw_read(&rw, &conn, sizeof(conn));
    //uint32_t wsession = 0;
    //memrw_read(&rw, &wsession, sizeof(wsession));
   
    char account[ACCOUNT_NAME_MAX+1];
    memrw_read(&rw, account, sizeof(account));

    struct user *ur = sh_hash64_find(&self->conn2user, conn);
    if (ur == NULL /*|| ur->wsession != wsession*/) {
        return;
    }
    if (memcmp(ur->account, account, sizeof(account))) {
        return;
    }
    int8_t type;
    memrw_read(&rw, &type, sizeof(type));

    int len;
    char cmd[1024], *tmp = cmd;
    
    int serr = SERR_UNKNOW;
    switch (type) {
    case T_QUERY: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_ARRAY) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        int n = item->value.i;
        if (n != 2) {
            serr = SERR_DBERR;
            break;
        }
        uint32_t accid = redis_bulkitem_toul(&item->child[0]);
        if (accid == 0) {
            len = redis_format(&tmp, sizeof(cmd), "incr acc:id");
            db(s, ur, T_GETID, cmd, len);
            //notify_login_fail(s, ur, SERR_NOACC);
            return;
        }
        if (!strncmp(ur->passwd, item->child[1].value.p, item->child[1].value.len)) {
            ur->accid = accid;
            serr = SERR_OK;
        } else {
            serr = SERR_ACCVERIFY;
        }
        break;
        }
    case T_GETID: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];        
        if (item->type != REDIS_REPLY_INTEGER) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        uint32_t accid = (uint32_t)item->value.u;
        if (accid > 0) {
            ur->accid = accid;
            len = redis_format(&tmp, sizeof(cmd), "hsetnx acc:%s id %u", ur->account, accid);
            db(s, ur, T_SETID, cmd, len);
            return;
        } else {
            serr = SERR_UNIQUEACCID;
        }
        break;
        }
    case T_SETID: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_INTEGER) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        int r = (uint32_t)item->value.u;
        if (r == 1) {
            len = redis_format(&tmp, sizeof(cmd), "hset acc:%s passwd %s", ur->account, PASSWD);
            db(s, ur, T_SETPASSWD, cmd, len);
            return;
        } else {
            serr = SERR_ACCEXIST;
        }
        break;
        }
    case T_SETPASSWD: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_INTEGER) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        serr = SERR_OK;
        break;
        }
    }

    if (serr == SERR_OK) {
        notify_login_ok(s, ur, ur->accid);
    } else {
        notify_login_fail(s, ur, serr);
    }
    sh_hash64_remove(&self->conn2user, conn);
    free(ur);
}

void
auth_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
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
    case MT_CMD:
        cmdctl(s, source, msg, sz, NULL);
        break;
    }
}

void
auth_time(struct module *s) {
    //struct auth *self = MODULE_SELF;
}
