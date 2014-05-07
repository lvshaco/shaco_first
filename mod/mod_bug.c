#include "sh.h"
#include "redis.h"
#include "msg_client.h"
#include "msg_server.h"
#include "memrw.h"

#define BUG_LEN 2048

struct bug {
    int source_handle;
    int rpbug_handle;
    struct redis_reply reply;
};

struct bug *
bug_create() {
    struct bug *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
bug_free(struct bug *self) {
    if (self == NULL)
        return;
    redis_finireply(&self->reply);
    free(self);
}

int 
bug_init(struct module *s) {
    struct bug *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("bug_gate", SUB_LOCAL, &self->source_handle) ||
        sh_handler("rpbug", SUB_LOCAL, &self->rpbug_handle)) {
        return 1;
    }
    redis_initreply(&self->reply, 512, 0);
    return 0;
}

static void
logout(struct module *s, int connid, int err) {
    struct bug *self = MODULE_SELF;
    
    UM_DEFWRAP(UM_GATE, ga, UM_BUGSUBMITRES, sr);
    ga->connid = connid;
    sr->err = err;
    sh_module_send(MODULE_ID, self->source_handle, MT_UM, ga, sizeof(*ga) + sizeof(*sr));

    UM_DEFWRAP(UM_GATE, ga2, UM_LOGOUT, lo);
    ga2->connid = connid;
    lo->err = SERR_OKUNFORCE;
    sh_module_send(MODULE_ID, self->source_handle, MT_UM, ga2, sizeof(*ga2) + sizeof(*lo));
}

static void
submit(struct module *s, int connid, const char *text, int sz) {
    if (sz <= 0 || sz > BUG_LEN) {
        logout(s, connid, SERR_BUGLEN);
        return;
    }
    struct bug *self = MODULE_SELF;
    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->flag = RQUERY_REPLY;
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &connid, sizeof(connid));
    rq->cbsz = RW_CUR(&rw);

    char cmd[BUG_LEN+128], *tmp = cmd;
    int len = redis_format(&tmp, sizeof(cmd), "LPUSH bug %b", text, sz);
    memcpy(rw.ptr, cmd, len);
    memrw_pos(&rw, len);

    int msgsz = sizeof(*rq) + RW_CUR(&rw);
    sh_module_send(MODULE_ID, self->rpbug_handle, MT_UM, rq, msgsz);
}

static void
redis(struct module *s, struct UM_REDISREPLY *rep, int sz) {
    struct bug *self = MODULE_SELF;
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));
    int connid = 0;
    memrw_read(&rw, &connid, sizeof(connid));
       
    int serr = SERR_UNKNOW;
    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
        serr = SERR_DBREPLY;
    } else {
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_INTEGER) {
            serr = SERR_OK;
        } else {
            serr = SERR_DBERR;
        }
    }
    logout(s, connid, serr);
}

void
bug_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct bug *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_GATE: {
            if (source != self->source_handle) {
                return;
            }
            UM_CASTCK(UM_GATE, ga, msg, sz);
            UM_CAST(UM_BASE, sub, ga->wrap);
            switch (sub->msgid) {
            case IDUM_BUGSUBMIT: {
                UM_CAST(UM_BUGSUBMIT, bs, sub);
                submit(s, ga->connid, bs->str, sz - sizeof(sub));
                break;
                }
            }
            break;
            }
        case IDUM_REDISREPLY: {
            UM_CAST(UM_REDISREPLY, rep, msg);
            redis(s, rep, sz);
            break;
            }
        }
        break;
        }
    }
}
