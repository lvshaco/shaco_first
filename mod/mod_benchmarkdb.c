#include "sh.h"
#include "redis.h"
#include "msg_server.h"
#include "memrw.h"

#define MODE_TEST  0
#define MODE_ACCA  1
#define MODE_ACCD  2
#define MODE_COIN  3

struct benchmarkdb {
    char mode[16];
    int startid;
    int curid;
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
benchmarkdb_init(struct module* s) {
    struct benchmarkdb* self = MODULE_SELF;
    redis_initreply(&self->reply, 512, 0);
 
    strncpy(self->mode, "test", sizeof(self->mode)-1);
    self->startid = 0;
    self->curid = self->startid;
    self->start = self->end = 0;
    self->query_init = sh_getint("benchmark_query_init", 50);
    self->query = sh_getint("benchmark_query", 10000);
    self->query_send = 0;
    self->query_recv = 0;
    self->query_done = 0;
    SUBSCRIBE_MSG(s->moduleid, IDUM_REDISREPLY);
    sh_timer_register(s->moduleid, 1000);
    return 0;
}

static void
_sendcmd(struct benchmarkdb* self, const char* cmd) {
    const struct sh_node* redisp = sh_node_get(HNODE_ID(NODE_RPUSER, 0));
    if (redisp == NULL) {
        sh_error("no redisproxy");
        return;
    }
    size_t len = strlen(cmd);
    UM_DEFVAR(UM_REDISQUERY, rq);
    rq->needreply = 1; 
    rq->needrecord = 0;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    memrw_write(&rw, cmd, len);
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    UM_SENDTONODE(redisp, rq, rq->msgsz);
    self->query_send++;
}

static void
_sendtest(struct benchmarkdb* self) {
    if (self->curid - self->startid >= self->query)
        self->curid = self->startid;
    int id = self->curid++;
    char cmd[1024];
    if (!strcmp(self->mode, "test")) {
        //_sendcmd(self, "hgetall user:1\r\n");
        _sendcmd(self, "get test\r\n");
        //_sendcmd(self, "zrange rank_score 0 -1 withscores\r\n");
    } else if (!strcmp(self->mode, "acca")) {
        snprintf(cmd, sizeof(cmd), "hmset acc:wa_account_%d id %d passwd 123456\r\n", id, id);
        _sendcmd(self, cmd);
    } else if (!strcmp(self->mode, "accd")) {
        snprintf(cmd, sizeof(cmd), "del acc:wa_account_%d\r\n", id);
        _sendcmd(self, cmd);
    } else if (!strcmp(self->mode, "coin")) {
        snprintf(cmd, sizeof(cmd), "hmset user:%d coin 1000000 diamond 100000\r\n", id);
        _sendcmd(self, cmd);
    } 
}

void
benchmarkdb_module(struct module* s, struct module_message* sm) {
    struct benchmarkdb* self = MODULE_SELF;
    const char* type = sm->msg;

    strncpy(self->mode, type, sizeof(self->mode)-1);

    self->start = sh_timer_now();
    self->startid = sm->sessionid;
    self->curid = self->startid;

    int count = sm->type;
    int init  = sm->sz;
    if (init <= 0)
        init =  1;
    self->query_init = min(count, init);
    self->query = count;
    self->query_send = 0;
    self->query_recv = 0;
    self->query_done = 0;
    int i;
    for (i=1; i<=count; ++i) {
        _sendtest(self);
    }
}

static void
_handleredisproxy(struct benchmarkdb* self, struct node_message* nm) {
    UM_CAST(UM_REDISREPLY, rep, nm->um);
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));

    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    assert(redis_getreply(&self->reply) == REDIS_SUCCEED);
    //redis_walkreply(&self->reply);
    self->query_done++;
    self->query_recv++;
    if (self->query_done == self->query) {
        self->end = sh_timer_now();
        uint64_t elapsed = self->end - self->start;
        if (elapsed == 0) elapsed = 1;
        float qps = self->query_done/(elapsed*0.001f);
        sh_info("query done: %d, query_send: %d, query_recv: %d, use time: %d, qps: %f", 
                self->query_done, self->query_send, self->query_recv, (int)elapsed, qps);
        self->start = self->end;
        self->query_done = 0;

    }
    _sendtest(self);
}

void
benchmarkdb_nodemsg(struct module* s, int id, void* msg, int sz) {
    struct benchmarkdb* self = MODULE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_RPUSER:
        _handleredisproxy(self, &nm);
        break;
    }
}

void
benchmarkdb_time(struct module* s) {
    struct benchmarkdb* self= MODULE_SELF;
    if (self->mode != MODE_TEST)
        return;
    if (self->query_send > 0)
        return;
    const struct sh_node* redisp = sh_node_get(HNODE_ID(NODE_RPUSER, 0));
    if (redisp == NULL)
        return;
    self->start = sh_timer_now();
    int i;
    for (i=0; i<self->query_init; ++i) {
        _sendtest(self);
    }
}
