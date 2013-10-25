#include "host_service.h"
#include "host.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_net.h"
#include "host_timer.h"
#include "host_assert.h"
#include "redis.h"
#include "user_message.h"
#include "node_type.h"
#include <stdlib.h>
#include <string.h>

struct benchmarkdb {
    uint64_t start;
    uint64_t end;
    int query;
    int query_send;
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
    self->query = host_getint("benchmark_query", 0);
    self->query_send = 0;
    self->query_done = 0;
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    host_timer_register(s->serviceid, 1000);
    return 0;
}

void
benchmarkdb_service(struct service* s, struct service_message* sm) {
    //struct benchmarkdb* self = SERVICE_SELF;
}

static void
_sendone(struct benchmarkdb* self) {
    const struct host_node* redisp = host_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (redisp == NULL) {
        return;
    }
    char* tmp;
    size_t len;
    //tmp = "*2\r\n$7\r\nhgetall\r\n$6\r\nuser:1\r\n";
    //host_net_send(self->connid, tmp, strlen(tmp));
    //tmp = "PING\r\n";
    tmp="hgetall user:1\r\n";
    len = strlen(tmp);
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->tag = 1;
    strncpy(rq->data, tmp, len);

    UM_SENDTONODE(redisp, rq, sizeof(*rq) + len);
    self->query_send++;
}

static void
_handleredisproxy(struct benchmarkdb* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_REDISREPLY);

    UM_CAST(UM_REDISREPLY, rep, nm->um);
    int sz = rep->msgsz - sizeof(*rep);
    char tmp[64*1024];
    strncpy(tmp, rep->data, sz);
    tmp[sz] = '\0';
    //host_error(tmp);
    
    redis_resetreplybuf(&self->reply, rep->data, sz);
    hassertlog(redis_getreply(&self->reply) == REDIS_SUCCEED);
    //redis_walkreply(&self->reply);
    self->query_done++;
    if (self->query_done == self->query) {
        self->end = host_timer_now();
        uint64_t elapsed = self->end - self->start;
        if (elapsed == 0) elapsed = 1;
        float qps = self->query_done/(elapsed*0.001f);
        host_info("query done: %d, use time: %d, qps: %f", 
                self->query_done, (int)elapsed, qps);
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
    const struct host_node* redisp = host_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (redisp == NULL) {
        return;
    }

    self->start = host_timer_now();
    int i;
    for (i=0; i<50; ++i) {
    _sendone(self);
    }
}
