#include "sc_service.h"
#include "sc_env.h"
#include "sc.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_timer.h"
#include "sc_assert.h"
#include "redis.h"
#include "user_message.h"
#include "node_type.h"
#include "memrw.h"
#include <stdlib.h>
#include <string.h>

struct benchmarkdb {
    uint64_t start;
    uint64_t end;
    int query_init;
    int query;
    int query_send;
    int query_recv;
    int query_done;
    struct redis_reply reply;
};

struct benchmarkdb*
benchmarkdb_create() {
    struct benchmarkdb* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
benchmarkdb_free(struct benchmarkdb* self) {
    redis_finireply(&self->reply);
    free(self);
}

int
benchmarkdb_init(struct service* s) {
    struct benchmarkdb* self = SERVICE_SELF;
    redis_initreply(&self->reply, 512, 0);
   
    self->start = self->end = 0;
    self->query_init = sc_getint("benchmark_query_init", 50);
    self->query = sc_getint("benchmark_query", 10000);
    self->query_send = 0;
    self->query_recv = 0;
    self->query_done = 0;
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    sc_timer_register(s->serviceid, 1000);
    return 0;
}

void
benchmarkdb_service(struct service* s, struct service_message* sm) {
    //struct benchmarkdb* self = SERVICE_SELF;
}

static void
_sendone(struct benchmarkdb* self) {
    const struct sc_node* redisp = sc_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (redisp == NULL) {
        sc_error("no redisproxy");
        return;
    }
    char* tmp;
    size_t len;
    //tmp = "*2\r\n$7\r\nhgetall\r\n$6\r\nuser:1\r\n";
    //sc_net_send(self->connid, tmp, strlen(tmp));
    //tmp = "PING\r\n";
    tmp="hgetall user:1\r\n";
    len = strlen(tmp);
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 1; 
    rq->needrecord = 0;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    memrw_write(&rw, tmp, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    UM_SENDTONODE(redisp, rq, rq->msgsz);
    self->query_send++;
}

static void
_handleredisproxy(struct benchmarkdb* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_REDISREPLY);
    UM_CAST(UM_REDISREPLY, rep, nm->um);
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));

    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    hassertlog(redis_getreply(&self->reply) == REDIS_SUCCEED);
    //redis_walkreply(&self->reply);
    self->query_done++;
    self->query_recv++;
    if (self->query_done == self->query) {
        self->end = sc_timer_now();
        uint64_t elapsed = self->end - self->start;
        if (elapsed == 0) elapsed = 1;
        float qps = self->query_done/(elapsed*0.001f);
        sc_info("query done: %d, query_send: %d, query_recv: %d, use time: %d, qps: %f", 
                self->query_done, self->query_send, self->query_recv, (int)elapsed, qps);
        self->start = self->end;
        self->query_done = 0;

    }
    _sendone(self);
}

void
benchmarkdb_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct benchmarkdb* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_REDISPROXY:
        _handleredisproxy(self, &nm);
        break;
    }
}

void
benchmarkdb_time(struct service* s) {
    struct benchmarkdb* self= SERVICE_SELF;
    if (self->query_send > 0) {
        return;
    }
    const struct sc_node* redisp = sc_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (redisp == NULL) {
        return;
    }

    self->start = sc_timer_now();
    int i;
    for (i=0; i<self->query_init; ++i) {
        _sendone(self);
    }
}
