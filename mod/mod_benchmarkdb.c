#include "sh.h"
#include "redis.h"
#include "msg_server.h"
#include "memrw.h"
#include "args.h"

#define MODE_TEST  0
#define MODE_ACCA  1
#define MODE_ACCD  2
#define MODE_COIN  3

struct benchmarkdb {
    int rpuser_handle;
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

    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("rpuser", SUB_REMOTE, &self->rpuser_handle)) {
        return 1;
    }
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
    
    sh_timer_register(s->moduleid, 1000);
    return 0;
}

static void
_sendcmd(struct module *s, const char* cmd) {
    struct benchmarkdb* self = MODULE_SELF;
    size_t len = strlen(cmd);
    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->needreply = 1; 
    rq->needrecord = 0;
    rq->cbsz = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, cmd, len);
    int msgsz = sizeof(*rq) + RW_CUR(&rw);
    sh_module_send(MODULE_ID, self->rpuser_handle, MT_UM, rq, msgsz);
    self->query_send++;
}

static void
_sendtest(struct module *s) {
    struct benchmarkdb *self = MODULE_SELF;
    if (self->curid - self->startid >= self->query)
        self->curid = self->startid;
    int id = self->curid++;
    char cmd[1024];
    if (!strcmp(self->mode, "test")) {
        snprintf(cmd, sizeof(cmd), "hmset user:%d coin 1000000 diamond 100000\r\n", id);
        _sendcmd(s, cmd);
    } else if (!strcmp(self->mode, "acca")) {
        snprintf(cmd, sizeof(cmd), "hmset acc:wa_account_%d id %d passwd 7c4a8d09ca3762af61e59520943dc26494f8941b\r\n", id, id);
        _sendcmd(s, cmd);
    } else if (!strcmp(self->mode, "accd")) {
        snprintf(cmd, sizeof(cmd), "del acc:wa_account_%d\r\n", id);
        _sendcmd(s, cmd);
    } else if (!strcmp(self->mode, "coin")) {
        snprintf(cmd, sizeof(cmd), "hmset user:%d coin 1000000 diamond 100000\r\n", id);
        _sendcmd(s, cmd);
    } 
}

void
command(struct module* s, const void *msg, int sz) {
    struct benchmarkdb* self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, sz);
    if (A.argc != 4) {
        return;
    }
    sh_strncpy(self->mode, A.argv[0], sizeof(self->mode));
    self->startid = strtol(A.argv[1], NULL, 10);
    int count = strtol(A.argv[2], NULL, 10);
    int init  = strtol(A.argv[3], NULL, 10);
    
    self->start = sh_timer_now();
    self->curid = self->startid;
    if (init <= 0)
        init =  1;
    self->query_init = min(count, init);
    self->query = count;
    self->query_send = 0;
    self->query_recv = 0;
    self->query_done = 0;
    int i;
    for (i=1; i<=count; ++i) {
        _sendtest(s);
    }
}

static void
process_redis(struct module *s, struct UM_REDISREPLY *rep, int sz) {
    struct benchmarkdb *self = MODULE_SELF;
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));

    redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
    assert(redis_getreply(&self->reply) == REDIS_SUCCEED);
    //redis_walkreply(&self->reply);
    self->query_done++;
    self->query_recv++;
    //sh_info("query %d, query_done %d", self->query, self->query_done);
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
    _sendtest(s);
}

void
benchmarkdb_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_REDISREPLY: {
            UM_CAST(UM_REDISREPLY, rep, msg);
            process_redis(s, rep, sz);
            break;
            }
        }
        break;
        }
    case MT_TEXT:
        command(s, msg, sz);
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
    self->start = sh_timer_now();
    int i;
    for (i=0; i<self->query_init; ++i) {
        _sendtest(s);
    }
}
