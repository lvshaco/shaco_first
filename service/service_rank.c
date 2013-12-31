#include "sc_service.h"
#include "sc_util.h"
#include "sc.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_timer.h"
#include "redis.h"
#include "user_message.h"
#include "node_type.h"
#include "player.h"
#include "sharetype.h"
#include "memrw.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct rank {
    uint32_t next_normal_refresh_time;
    uint32_t next_dashi_refresh_time;
    struct redis_reply reply;
};

struct rank*
rank_create() {
    struct rank* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
rank_free(struct rank* self) {
    redis_finireply(&self->reply);
    free(self);
}

int
rank_init(struct service* s) {
    struct rank* self = SERVICE_SELF;
    
    redis_initreply(&self->reply, 512, 0);
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    return 0;
}

static int
_send_to_db(struct UM_REDISQUERY* rq) {
    const struct sc_node* db = sc_node_get(HNODE_ID(NODE_RPRANK, 0));
    if (db) {
        UM_SENDTONODE(db, rq, rq->msgsz);
        return 0;
    }
    return 1;
}

static int
_refresh_rank(const char* type, time_t now, uint32_t* base) {
    char strtime[24];
    struct tm tmnow  = *localtime(&now); 
    strftime(strtime, sizeof(strtime), "%y%m%d-%H:%M:%S", &tmnow);

    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 0;
    rq->needrecord = 1;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    int len = snprintf(rw.ptr, RW_SPACE(&rw), 
            "MULTI\r\n"
            "SET rank:%s_refresh_time %s\r\n"
            "ZUNIONSTORE rank:%s_%s 1 rank:%s\r\n"
            "EXEC\r\n",
            type, strtime, type, strtime, type);
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}

static int
_insert_rank(const char* type, const char* oldtype, uint32_t charid, uint64_t score) { 
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 0;
    rq->needrecord = 1;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    int len;
    if (oldtype[0] == '\0') {
        if (type[0]) {
            len = snprintf(rw.ptr, RW_SPACE(&rw), 
                    "ZADD rank:%s %llu %u\r\n"
                    "ZREMRANGEBYRANK rank:%s -100001 -100001\r\n",
                    type, (unsigned long long int)score, (unsigned int)charid, type);
        } else {
            return 1;
        }
    } else {
        if (type[0]) {
            len = snprintf(rw.ptr, RW_SPACE(&rw), 
                    "ZREM rank:%s %u\r\n"
                    "ZADD rank:%s %llu %u\r\n"
                    "ZREMRANGEBYRANK rank:%s -100001 -100001\r\n",
                    oldtype, (unsigned int)charid,
                    type, (unsigned long long int)score, (unsigned int)charid, type);
        } else {
            len = snprintf(rw.ptr, RW_SPACE(&rw), 
                    "ZREM rank:%s %u\r\n",
                    oldtype, (unsigned int)charid);
        }
    }
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}
/*
static int
_query_refresh_time(const char* type) {
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 1;
    rq->needrecord = 0;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    uint8_t l = strlen(type);
    memrw_write(&rw, &l, sizeof(l));
    memrw_write(&rw, type, l);
    rq->cbsz = RW_CUR(&rw);
    int len = snprintf(rw.ptr, RW_SPACE(&rw), 
            "GET rank:%s_refresh_time\r\n", type);
    memrw_pos(&rw, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    return _send_to_db(rq);
}
*/
static void
_refresh_db(struct rank* self) {
    uint32_t now = sc_timer_now()/1000;
    uint32_t base; 
    if (self->next_normal_refresh_time <= now) {
        if (_refresh_rank("normal", now, &base) == 0) {
            self->next_normal_refresh_time = base + 7 * SC_DAY_SECS;
        }
    }
    if (self->next_dashi_refresh_time <= now) {
        if (_refresh_rank("dashi",  now, &base) == 0) {
            self->next_dashi_refresh_time = base + 7 * SC_DAY_SECS;
        }
    }
}

void
rank_service(struct service* s, struct service_message* sm) {
    //struct rank* self = SERVICE_SELF;
    const char* type = sm->p1;
    const char* oldtype = sm->p2;
    uint32_t charid = sm->i1;
    uint64_t score = sm->n1;
    _insert_rank(type, oldtype, charid, score);
}

static struct redis_replyitem*
_get_replystringitem(struct redis_reply* reply) {
    if (redis_getreply(reply) == REDIS_SUCCEED) {
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
    struct tm tmlast; 
    //strptime(strtime, "%Y%m%d-%H:%M:%S", &tmlast);
    sscanf(strtime, "%4d%2d%2d-%2d:%2d:%2d", 
            &tmlast.tm_year, &tmlast.tm_mon, &tmlast.tm_mday,
            &tmlast.tm_hour, &tmlast.tm_min, &tmlast.tm_sec);
    tmlast.tm_year -= 1900;
    tmlast.tm_mon  -= 1;
    time_t last = mktime(&tmlast);
    return last; 
}

static void
_handle_redis(struct rank* self, struct node_message* nm) {
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
    if (!strcmp(type, "normal")) {
        time_t last = _get_timevalue(si); 
        struct tm tmlast = *localtime(&last);
        time_t base = sc_day_base(last, tmlast);
        self->next_normal_refresh_time = base + SC_DAY_SECS * 7;
    } else if (!strcmp(type, "dashi")) {
        time_t last = _get_timevalue(si);
        struct tm tmlast = *localtime(&last);
        time_t base = sc_day_base(last, tmlast);
        self->next_dashi_refresh_time = base + SC_DAY_SECS * 7;
    }
}

void
rank_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct rank* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_RPRANK:
        _handle_redis(self, &nm);
        break;
    }
}

void
rank_time(struct service* s) {
    struct rank* self = SERVICE_SELF;
    _refresh_db(self);
}
