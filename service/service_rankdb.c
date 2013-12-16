#include "sc_service.h"
#include "sc_util.h"
#include "sc.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_timer.h"
#include "sc_assert.h"
#include "redis.h"
#include "user_message.h"
#include "node_type.h"
#include "player.h"
#include "rankdb.h"
#include "sharetype.h"
#include "memrw.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct rankdb {
    //int requester;
    uint32_t next_score_refresh_time;
    uint32_t next_race_refresh_time;
    struct redis_reply reply;
};

struct rankdb*
rankdb_create() {
    struct rankdb* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
rankdb_free(struct rankdb* self) {
    redis_finireply(&self->reply);
    free(self);
}

int
rankdb_init(struct service* s) {
    struct rankdb* self = SERVICE_SELF;
    //if (sc_handler("world", &self->requester))
        //return 1;
    
    redis_initreply(&self->reply, 512, 0);
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    return 0;
}

static int
_send_to_db(struct UM_REDISQUERY* rq) {
    const struct sc_node* db = sc_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (db) {
        UM_SENDTONODE(db, rq, rq->msgsz);
        return 0;
    }
    return 1;
}

static int
_refresh_rank(const char* type, uint32_t now, uint32_t* base) {
    char strtime[24];
    tm tmnow  = *localtime(&now); 
    strftime(strtime, sizeof(strtime), "%y%m%d-%H:%M:%S", &tmnow);
    *base = sc_day_base(now, tmnow);

    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 0;
    rq->needrecord = 1;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    int len = snprintf(rw.ptr, RW_SPACE(&rw), 
            "MULTI\r\n"
            "SET rank:%s_refresh_time %s\r\n"
            "ZUNIONSTORE rank:%s_%s 1 rank:%s"
            "EXEC\r\n",
            type, strtime, type, strtime, type);
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}

static int
_insert_rank(const char* type, uint32_t charid, uint64_t score) {
    char strtime[24];
    tm tmnow  = *localtime(&now); 
    strftime(strtime, sizeof(strtime), "%Y%m%d-%H:%M:%S", &tmnow);
    *base = sc_day_base(now, tmnow);

    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 0;
    rq->needrecord = 1;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    int len = snprintf(rw.ptr, RW_SPACE(&rw), 
            "ZADD rank:%s %ul %u\r\n"
            "ZREMRANGEBYRANK rank_score -101 -10000000\r\n",
            type, score, charid);
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}

static int
_query_refresh_time(const char* type) {
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 1;
    rq->needrecord = 0;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    uint8_t len = strlen(type);
    memrw_write(&rw, len, sizeof(len));
    memrw_write(&rw, type, len);
    rq->cbsz = RW_CUR(&rw);
    int len = snprintf(rw.ptr, RW_SPACE(&rw), 
            "GET rank:%s_refresh_time\r\n", type);
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}

static void
_refresh_db(struct rankdb* self) {
    uint32_t now = host_timer_now()/1000;
    uint32_t base; 
    if (self->next_score_refresh_time <= now) {
        if (_refresh_rank("score", now, &base) == 0) {
            self->next_score_refresh_time = base + 7 * SC_DAY_SECS;
        }
    }
    if (self->next_race_refresh_time <= now) {
        if (_refresh_rank("race",  now, &base) == 0) {
            self->next_race_refresh_time = base + 7 * SC_DAY_SECS;
        }
    }
}

void
rankdb_service(struct service* s, struct service_message* sm) {
    //struct rankdb* self = SERVICE_SELF;
    struct rankdbcmd* cmd = sm->msg;
    _insert_rank(sm->msg, cmd->charid, cmd->score);
}

static struct redis_replyitem*
_get_replystringitem(struct redis_reply* reply) {
    if (redis_getreply(&self->reply) == REDIS_SUCCEED) {
        struct redis_replyitem* item = reply->stack[0];
        if (item->type == REDIS_REPLY_STRING)
            return item;
    }
    return NULL;
}

static time_t
_get_timevalue(struct redis_replyitem* si) {
    char strtime[24];
    strncpychk(strtime, sizeof(strtime), si->value.p, si->value.len);
    tm tmlast;
    strptime(strtime, "%Y%m%d-%H:%M:%S", &tmlast);
    time_t last = mktime(&tmlast);
    return last; 
}

static void
_handle_redis(struct rankdb* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_REDISREPLY);
    UM_CAST(UM_REDISREPLY, rep, nm->um);
        
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));
    uint8_t len; 
    memrw_read(&rw, &len, sizeof(len));
    char type[(int)len+1];
    memrw_read(&rw, type, len);
    type[len] = '\0';

    struct redis_replyitem* si;
    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    si = _get_replystringitem(&self->reply);
    if (si == NULL) {
        return;
    } 
    if (!strcmp(type, "score")) {
        time_t last = _get_valuetime(si); 
        time_t base = sc_day_base(last, tmlast);
        self->next_race_refresh_time = base + SC_DAY_SECS * 7;
    } else if (!strcmp(type, "race")) {
        time_t last = _get_valuetime(si);
        time_t base = sc_day_base(last, tmlast);
        self->next_race_refresh_time = base + SC_DAY_SECS * 7;
    }
}

void
rankdb_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct rankdb* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_REDISPROXY:
        _handle_redis(self, &nm);
        break;
    }
}

void
rankdb_time(struct service* s) {
    struct rankdb* self = SERVICE_SELF;
    refresh_db(self);
}
