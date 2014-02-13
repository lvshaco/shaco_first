#include "sh.h"
#include "msg_server.h"
#include "redis.h"
#include "freelist.h"
#include "memrw.h"
#include <stdbool.h>

struct querylink {
    struct querylink* next;
    int source;
    uint16_t needreply:1;
    uint16_t cbsz:15;
    char cb[];
};

struct queryqueue {
    FREELIST(querylink);
};

struct redisproxy {
    int connid;
    struct redis_reply reply;
    struct queryqueue queryq;
    int maxcount;
    int allcount;
    int times;
};

struct redisproxy*
redisproxy_create() {
    struct redisproxy* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
redisproxy_free(struct redisproxy* self) {
    redis_finireply(&self->reply);
    FREELIST_FINI(querylink, &self->queryq);
    free(self);
}

static int
block_connect_redis(struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    const char* addr = sh_getstr("redis_ip", "");
    int port = sh_getint("redis_port", 0);
    int err; 
    int connid = sh_net_block_connect(addr, port, MODULE_ID, 0, &err);
    if (connid < 0) {
        sh_error("Connect to redis %s:%u fail: %s", addr, port, sh_net_error(err));
        return 1;
    } else {
        self->connid = connid;
        sh_net_subscribe(connid, true);
        sh_info("Connect to redis %s:%u ok", addr, port);
        return 0;
    }
}

static int
block_read(int id, struct redis_reply *reply) {
    int e;
    for (;;) {
        void* buf = REDIS_REPLYBUF(reply);
        int space = REDIS_REPLYSPACE(reply);
        if (space <= 0) {
            return NET_ERR_NOBUF;
        }
        int nread = sh_net_readto(id, buf, space, &e);
        if (e) {
            return e;
        }
        if (nread <= 0)
            continue;

        reply->reader.sz += nread;
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

static int
auth(struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    int id = self->connid;
    int err;
    const char* auth = sh_getstr("redis_auth", "123456");
    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp), "AUTH %s\r\n", auth);
    if (sh_net_block_send(id, tmp, len, &err) != len) {
        sh_error("auth fail: %s", sh_net_error(err)); 
        return 1;
    }
    struct redis_reply reply;
    redis_initreply(&reply, 512, 16*1024);
    block_read(id, &reply);

    if (!redis_to_status(REDIS_ITEM(&reply))) {
        char tmp[1024];
        redis_to_string(REDIS_ITEM(&reply), tmp, sizeof(tmp));
        sh_error("redis auth error: %s", tmp);
        return 1;
    }
    redis_finireply(&reply);
    sh_info("login redis");
    return 0;
}

int
redisproxy_init(struct module* s) {
    struct redisproxy* self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    int handle;
    if (sh_handler(sh_getstr("redis_requester", ""), SUB_REMOTE, &handle)) {
        return 1;
    }
    self->connid = -1;
    if (block_connect_redis(s)) {
        return 1;
    }
    if (auth(s)) {
        return 1;
    }
    redis_initreply(&self->reply, 512, 16*1024);
    FREELIST_INIT(&self->queryq);

    sh_timer_register(s->moduleid, 1000);
    return 0;
}

static void
query(struct module *s, int source, struct UM_REDISQUERY *rq, int sz) {
    struct redisproxy *self = MODULE_SELF;

    int datasz = sz - (int)sizeof(*rq) - (int)rq->cbsz;
    if (datasz < 3) {
        return; // need 3 bytes at least
    }
    int cbsz = rq->cbsz;
    char* dataptr = rq->data + cbsz;
    int lastpos = 0; 
    int n = 0;
    int i;
    for (i=0; i<datasz-1;) {
        if (memcmp(&dataptr[i], "\r\n", 2) == 0) {
            n++;
            i+=2;
            lastpos = i;
        } else {
            i++;
        }
    }
    if (lastpos != datasz) {
        return; // need endswith \r\n
    }
    char* cbptr = rq->data;
    for (i=0; i<n; ++i) {
        struct querylink* ql = FREELIST_PUSH(querylink, 
                                             &self->queryq, 
                                             sizeof(struct querylink) + cbsz);
        ql->source = source;
        ql->needreply = rq->needreply;
        ql->cbsz = cbsz;
        if (cbsz > 0) {
            memcpy(ql->cb, cbptr, cbsz);
        }
    }
    if (self->connid != -1) {
        char *tmp = malloc(datasz);
        memcpy(tmp, dataptr, datasz);
        sh_net_send(self->connid, tmp, datasz);
    } else {
        if (rq->needrecord) {
            dataptr[datasz-1] = '\0';
            sh_rec(dataptr);
        }
    }
}

void
redisproxy_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_REDISQUERY: {
            UM_CAST(UM_REDISQUERY, rq, msg);
            query(s, source, rq, sz);
            }
        }
        break;
        }
    }
}

static void
handle_reply(struct module *s) {
    struct redisproxy *self = MODULE_SELF;
    //redis_walkreply(&self->reply); // todo: delete

    struct querylink* ql = FREELIST_POP(querylink, &self->queryq);
    assert(ql);
    if (ql->needreply == 0) {
        return; // no need reply
    }
    UM_DEFVAR2(UM_REDISREPLY, rep, UM_MAXSZ);
    rep->cbsz = ql->cbsz;
   
    struct redis_reader* reader = &self->reply.reader;
    struct memrw rw;
    memrw_init(&rw, rep->data, UM_MAXSZ - sizeof(*rep));
    if (ql->cbsz) {
        memrw_write(&rw, ql->cb, ql->cbsz);
    }
    memrw_write(&rw, reader->buf+reader->pos_last, reader->pos-reader->pos_last);
    int msgsz = RW_CUR(&rw) + sizeof(*rep);
    sh_module_send(MODULE_ID, ql->source, MT_UM, rep, msgsz);
}

static void
read(struct module *s, struct net_message* nm) {
    struct redisproxy* self = MODULE_SELF;
    assert(nm->connid == self->connid);
    int id = nm->connid;
    int e = 0;
    struct redis_reply* reply = &self->reply;

    for (;;) {
        void* buf = REDIS_REPLYBUF(reply);
        int space = REDIS_REPLYSPACE(reply);
        if (space <= 0) {
            e = NET_ERR_NOBUF;
            goto errout;
        }
        int nread = sh_net_readto(id, buf, space, &e);
        if (nread <= 0) {
            goto errout;
        }
        reply->reader.sz += nread;
        int result = redis_getreply(reply);
        int K = 0;
        while (result == REDIS_SUCCEED) {
            handle_reply(s);
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
    }
    return; 
errout:
    if (e) {
        sh_net_close_socket(id, true);
        nm->type = NETE_SOCKERR;
        nm->error = e;
        module_net(nm->ud, nm);
    }
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
        FREELIST_POPALL(querylink, &self->queryq);
        sh_error("connect to redis fail: %s", sh_net_error(nm->error)); 
        break; */
    case NETE_SOCKERR:
        self->connid = -1;
        FREELIST_POPALL(querylink, &self->queryq);
        sh_error("redis disconnect: %s", sh_net_error(nm->error));
        break;
    }
}

void
redisproxy_time(struct module* s) {
    struct redisproxy* self = MODULE_SELF;
    if (self->connid == -1) {
        if (!block_connect_redis(s)) {
            auth(s);
        }
    }
    if (self->times > 0) {
        //sh_info("maxcount = %d, agvcount = %d", self->maxcount, self->allcount/self->times);
    }
}
