#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"
#include "redis.h"
#include "memrw.h"
#include <stdbool.h>

struct query {
    struct query* next;
    int source;
    uint16_t reply:1;
    uint16_t cbsz:15;
    char cb[];
};

struct qlist {
    struct query *head;
    struct query *tail;
};

struct instance {
    char ip[40];
    int port;
    int connid;
    struct redis_reply reply;
    struct qlist ql;
};

struct redisproxy {
    int ninst;
    struct instance *insts;
    int maxcount;
    int allcount;
    int times;
};

static const char *
field_name(struct module *s, const char *name, char fname[64]) {
    fname[0] = '\0';
    strcat(fname, MODULE_NAME);
    strcat(fname, "_");
    strcat(fname, name);
    return fname;
}

#define FIELD(name) field_name(s, name, field)

static int
block_read(int id, struct redis_reply *reply) {
    int e;
    for (;;) {
        void* buf = REDIS_REPLYBUF(reply);
        int space = REDIS_REPLYSPACE(reply);
        if (space <= 0) {
            return NET_ERR_NOBUF;
        }
        int n = sh_net_readto(id, buf, space, &e);
        if (e) {
            return e;
        }
        if (n <= 0)
            continue;

        reply->reader.sz += n;
        int result = redis_getreply(reply);
       
        switch (result) {
        case REDIS_SUCCEED:
            return 0;
        case REDIS_NEXTTIME:
            break;
        default:
            return NETE_REDISREPLY;
        }
    }
    return 0;
}

// qlist
static void
qlist_init(struct qlist *ql) {
    ql->head = NULL;
    ql->tail = NULL;
}

static void
qlist_fini(struct qlist *ql) {
    struct query *q;
    while (ql->head) {
        q = ql->head;
        ql->head = ql->head->next;
        free(q);
    }
    ql->head = NULL;
    ql->tail = NULL;
}

static void
qlist_push(struct qlist *ql, struct query *q) {
    if (ql->head == NULL) {
        ql->head = q;
    } else {
        assert(ql->tail != NULL);
        assert(ql->tail->next == NULL);
        ql->tail->next = q;
    } 
    ql->tail = q;
    q->next = NULL;
}

static struct query *
qlist_pop(struct qlist *ql) {
    if (ql->head) {
        struct query *q;
        q = ql->head;
        ql->head = ql->head->next;
        return q;
    } else {
        return NULL;
    }
}

// instance
static int
instance_addr(struct instance *inst, const char *addr) {
    char tmp[64];
    sh_strncpy(tmp, addr, sizeof(tmp));
    char *p = strchr(tmp, ':');
    if (p == NULL) {
        return 1;
    }
    *p = '\0';
    sh_strncpy(inst->ip, tmp, sizeof(inst->ip));
    inst->port = strtol(p+1, NULL, 10);
    return 0;
}

static int
instance_init(struct instance *inst) {
    inst->connid = -1;    
    redis_initreply(&inst->reply, 512, 16*1024);
    qlist_init(&inst->ql);
    return 0;
}

static void
instance_fini(struct instance *inst) {
    inst->connid = -1;
    redis_finireply(&inst->reply);
    qlist_fini(&inst->ql); 
}

static inline int
instance_slot(struct instance *inst, struct redisproxy *self) {
    return inst - self->insts;
}

static inline const char *
instance_string(struct instance *inst, struct redisproxy *self, char *str, int len) {
    sh_snprintf(str, len, "instance#%d %s:%d", 
            instance_slot(inst, self), inst->ip, inst->port);
    return str;
}

#define strinst instance_string(inst, self, tmp, sizeof(tmp))

static void
instance_disconnect(struct instance *inst, struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    if (inst->connid != -1) {
        char tmp[128];
        sh_net_close_socket(inst->connid, true);
        inst->connid = -1;
        sh_info("Redis %s disconnect active", strinst);
    }
}

static int
instance_connect(struct instance *inst, struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    char tmp[256];
    int err; 
    int connid = sh_net_block_connect(inst->ip, inst->port, MODULE_ID, 
            instance_slot(inst, self), &err);
    if (connid < 0) {
        sh_error("Redis %s connect fail: %s", strinst, sh_net_error(err));
        return 1;
    } else {
        inst->connid = connid;
        sh_net_subscribe(connid, true);
        return 0;
    }
}

static int
instance_auth(struct instance *inst, struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    int id = inst->connid;
    int err, len;
    char field[64];
    const char* auth = sh_getstr(FIELD("redis_auth"), "");
    char tmp[128];
    char cmd[128], *pcmd = cmd;

    len = redis_format(&pcmd, sizeof(cmd), "AUTH %s", auth);
    if (sh_net_block_send(id, pcmd, len, &err) != len) {
        sh_error("Redis %s auth fail: %s", strinst, sh_net_error(err)); 
        return 1;
    }
    struct redis_reply reply;
    redis_initreply(&reply, 512, 16*1024);
    block_read(id, &reply);

    if (!redis_to_status(REDIS_ITEM(&reply))) {
        char strerr[1024];
        redis_to_string(REDIS_ITEM(&reply), strerr, sizeof(strerr));
        sh_error("Redis %s auth error: %s", strinst, strerr);
        return 1;
    }
    redis_finireply(&reply);
    return 0;
}

static int
instance_login(struct instance *inst, struct module *s) {
    char tmp[128];
    struct redisproxy *self = MODULE_SELF;
    if (!instance_connect(inst, s) &&
        !instance_auth(inst, s)) {
        sh_info("Redis %s login", strinst);
        return 0;
    } else {
        sh_error("Redis %s login fail", strinst);
        return 1;
    }
}

static int
instance_reset(struct instance *inst, struct module *s) {
    instance_disconnect(inst, s);
    instance_fini(inst);
    instance_init(inst);
    return instance_login(inst, s);
}

static inline int
instance_send(struct instance *inst, const char *cmd, int len) {
    if (inst->connid != -1) {
        char *tmp = malloc(len);
        memcpy(tmp, cmd, len);
        return sh_net_send(inst->connid, tmp, len);
    } else 
        return 1;
}

static inline struct instance *
instance_get(struct redisproxy *self, int slot, int connid) {
    assert(slot >= 0 && slot < self->ninst);
    struct instance *inst = &self->insts[slot];
    assert(inst->connid == connid);
    return inst;
}

// redisproxy
struct redisproxy*
redisproxy_create() {
    struct redisproxy* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
redisproxy_free(struct redisproxy* self) {
    int i;
    for (i=0; i<self->ninst; ++i) {
        instance_fini(&self->insts[i]);
    }
    free(self);
}

static int
init_requester(struct module *s) {
    char field[64];
    
    char *one, *ptr;
    const char *requester = sh_getstr(FIELD("requester"), "");
    char tmp[1024];
    
    ptr= NULL;
    sh_strncpy(tmp, requester, sizeof(tmp));
    one = strtok_r(tmp, ",", &ptr);
    while (one) {
        int handle;
        if (sh_handle_subscribe(one, SUB_REMOTE, &handle)) {
            return 1;
        }
        one = strtok_r(NULL, ",", &ptr);
    }
    return 0;
}

static int
init_instances(struct module *s) {
    struct redisproxy *self = MODULE_SELF;

    char field[64];
    int sharding_mod = sh_getint(FIELD("sharding_mod"), 1);
    if (sharding_mod <= 0 ||
        sharding_mod & (sharding_mod-1)) {
        sh_error("Redis sharding_mod must pow of 2");
        return 1;
    }
    char buf[64*64];
    const char *list = sh_getstr(FIELD("redis_list"), "");
    if (list[0] == '\0') {
        sh_error("Redis no instance");
        return 1;
    }
    sh_strncpy(buf, list, sizeof(buf));

    self->ninst = sharding_mod;
    self->insts = malloc(sizeof(self->insts[0]) * self->ninst);
    int i;
    for (i=0; i<self->ninst; ++i) {
        instance_init(&self->insts[i]);
    }
    struct instance *inst;
    int n = 0; 
    char *one, *ptr;
    ptr = NULL;
    one = strtok_r(buf, ",", &ptr);
    while (one) {
        if (n >= self->ninst) {
            sh_error("Redis sharding_mod must equal instance count");
            return 1;
        }
        inst = &self->insts[n++];
        if (instance_addr(inst, one)) {
            sh_error("Redis instance `%s` init fail", one);
            return 1;
        }
        if (instance_login(inst, s)) {
            return 1;
        }
        one = strtok_r(NULL, ",", &ptr);
    }
    return 0;
}

int
redisproxy_init(struct module* s) {
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (init_requester(s) ||
        init_instances(s)) {
        return 1;
    }
    sh_timer_register(s->moduleid, 1000);
    return 0;
}

static void
response(struct module *s, int source, void *cb, int cbsz, void *data, int sz) {
    UM_DEFVAR2(UM_REDISREPLY, rep, UM_MAXSZ);
    rep->cbsz = cbsz;
   
    struct memrw rw;
    memrw_init(&rw, rep->data, UM_MAXSZ - sizeof(*rep));
    if (cbsz) {
        memrw_write(&rw, cb, cbsz);
    }
    memrw_write(&rw, data, sz);
    int msgsz = RW_CUR(&rw) + sizeof(*rep);
    sh_handle_send(MODULE_ID, source, MT_UM, rep, msgsz);
}

static void
handle_query(struct module *s, int source, struct UM_REDISQUERY *rq, int sz) {
    struct redisproxy *self = MODULE_SELF;

    uint8_t* cb = (uint8_t*)rq->data;
    int cbsz    = rq->cbsz;
    char* cmd   = rq->data + cbsz;
    int cmdlen  = sz - (int)sizeof(*rq) - (int)rq->cbsz;
    if (cmdlen < 5) {
        response(s, source, cb, cbsz, SH_LITERAL("-XX"));
        return; // need 5 bytes at least *0\r\n
    }
    uint32_t slot, key;
    if (rq->flag & RQUERY_SHARDING) {
        assert(cbsz >= 4);
        key = *((uint32_t*)cb); //sh_from_littleendian32(cb);
        slot = key & (self->ninst - 1);
    } else {
        slot = 0;
    }
    struct instance *inst;
    inst = &self->insts[slot];
   
    if (!instance_send(inst, cmd, cmdlen)) {
        struct query *q = malloc(sizeof(*q) + cbsz);
        q->source = source;
        q->reply = (rq->flag & RQUERY_REPLY) ? 1 : 0;
        q->cbsz = cbsz;
        if (cbsz > 0) {
            memcpy(q->cb, cb, cbsz);
        }
        qlist_push(&inst->ql, q);
    } else {
        response(s, source, cb, cbsz, SH_LITERAL("-RD")); // disconnected
        if (rq->flag & RQUERY_BACKUP) {
            cmd[cmdlen-1] = '\0';
            sh_rec(cmd);
        }
    }
}

static void
handle_reply(struct module *s, struct instance *inst) {
    //struct redisproxy *self = MODULE_SELF;
    //redis_walkreply(&self->reply); // todo: delete

    struct redis_reader *reader = &inst->reply.reader;
    struct query *q = qlist_pop(&inst->ql);
    assert(q);
    if (q->reply != 0) {
        response(s, q->source, q->cb, q->cbsz, 
                reader->buf+reader->pos_last, 
                reader->pos-reader->pos_last);
    }
    free(q);
}

static void
read(struct module *s, struct net_message* nm) {
    struct redisproxy* self = MODULE_SELF;
    int id = nm->connid;
    int e = 0;

    struct instance *inst = instance_get(self, nm->ut, id);
    assert(inst);

    struct redis_reply* reply = &inst->reply;

    for (;;) {
        void* buf = REDIS_REPLYBUF(reply);
        int space = REDIS_REPLYSPACE(reply);
        if (space <= 0) {
            e = NET_ERR_NOBUF;
            goto errout;
        }
        int nread = sh_net_readto(id, buf, space, &e);
        if (nread > 0) {
            reply->reader.sz += nread;
            int result = redis_getreply(reply);
            int K = 0;
            while (result == REDIS_SUCCEED) {
                handle_reply(s, inst);
                redis_resetreply(reply); 
                result = redis_getreply(reply);
                K++;
            }
            redis_resetreply(reply);
            if (K > 0) {
                self->times++;
                self->allcount+=K;
                if (self->maxcount < K)
                    self->maxcount = K;
            }
            if (result == REDIS_ERROR) {
                e = NETE_REDISREPLY;
                goto errout;
            }
            if (nread < space) {
                break; // net read over
            }
        } else if (nread < 0) {
            goto errout;
        } else {
            goto out;
        }
    }
out:
    return; 
errout:
    sh_net_close_socket(id, true);
    nm->type = NETE_SOCKERR;
    nm->error = e;
    module_net(nm->ud, nm);
}

void
redisproxy_net(struct module* s, struct net_message* nm) {
    struct redisproxy* self = MODULE_SELF;
    switch (nm->type) {
    case NETE_READ:
        read(s, nm);
        break;
    /*case NETE_CONNECT:
        self->connid = nm->connid;
        sh_net_subscribe(nm->connid, true);
        sh_info("connect to redis ok");
        break;
    case NETE_CONNERR:
        self->connid = -1;
        FREELIST_POPALL(query, &self->queryl);
        sh_error("connect to redis fail: %s", sh_net_error(nm->error)); 
        break; */
    case NETE_SOCKERR: {
        struct instance *inst = instance_get(self, nm->ut, nm->connid);
        assert(inst);
        char tmp[128];
        sh_error("Redis %s disconnect: %s", strinst, sh_net_error(nm->error));
        instance_fini(inst);
        instance_init(inst);
        }
        break;
    }
}

void
redisproxy_time(struct module* s) {
    struct redisproxy* self = MODULE_SELF;
  
    struct instance *inst;
    int i;
    for (i=0; i<self->ninst; ++i) {
        inst = &self->insts[i];
        if (inst->connid == -1) {
            instance_login(inst, s);
        }
    }
    if (self->times > 0) {
        //sh_info("maxcount = %d, agvcount = %d", self->maxcount, self->allcount/self->times);
    }
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    struct redisproxy *self = MODULE_SELF;
 
    struct instance *inst;

    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "reset")) {
        if (A.argc < 2) {
            return CTL_ARGLESS;
        }
        int nfail = 0;
        uint32_t slot;
        char *list = A.argv[1];
        char *one, *ptr;
        one = strtok_r(list, ",", &ptr);
        while (one) {
            slot = strtoul(one, NULL, 10);
            if (slot < self->ninst) {
                inst = &self->insts[slot];
                instance_reset(inst, s);
            } else {
                nfail++;
                sh_error("Redis [%s] invalid slot %u", cmd, slot);
            }
            one = strtok_r(NULL, ",", &ptr);
        }
        if (nfail > 0) {
            return CTL_SOMEFAIL;
        }
    } else if (!strcmp(cmd, "resetall")) {
        int i;
        for (i=0; i<self->ninst; ++i) {
            inst = &self->insts[i];
            instance_reset(inst, s);
        }
    } else if (!strcmp(cmd, "list")) {
        char tmp[128];
        int i;
        for (i=0; i<self->ninst; ++i) {
            inst = &self->insts[i];
            int n;
            n = sh_snprintf(rw->ptr, RW_SPACE(rw), "\r\n  ");
            memrw_pos(rw, n);
            n = sh_snprintf(rw->ptr, RW_SPACE(rw), strinst);
            memrw_pos(rw, n);
        }
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

void
redisproxy_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_REDISQUERY: {
            UM_CAST(UM_REDISQUERY, rq, msg);
            handle_query(s, source, rq, sz);
            break;
            }
        }
        break;
        }
    case MT_CMD:
        cmdctl(s, source, msg, sz, command);
        break;
    }
}
