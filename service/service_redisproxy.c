#include "sc_service.h"
#include "sc_env.h"
#include "sc.h"
#include "sc_dispatcher.h"
#include "sc_timer.h"
#include "sc_log.h"
#include "sc_net.h"
#include "user_message.h"
#include "client_type.h"
#include "redis.h"
#include "freelist.h"
#include "memrw.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct querylink {
    struct querylink* next;
    uint16_t nodeid;
    uint16_t needreply:1;
    uint16_t cbsz:15;
    char cb[];
};

struct queryqueue {
    FREELIST(querylink);
};

struct redisproxy {
    int connid;
    bool authed;
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
_connect_redis(struct service* s, bool block) {
    const char* addr = sc_getstr("redis_ip", "");
    int port = sc_getint("redis_port", 0);
    sc_info("connect to redis %s:%u ...", addr, port);
    if (sc_net_connect(addr, port, block, s->serviceid, CLI_REDIS)) { 
        return 1;
    }
    return 0;
}

static int
_redis_read(int id, struct redis_reply* reply) {
    int e;
    for (;;) {
        void* buf = REDIS_REPLYBUF(reply);
        int space = REDIS_REPLYSPACE(reply);
        if (space <= 0) {
            return NET_ERR_NOBUF;
        }
        int nread = sc_net_readto(id, buf, space, &e);
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
_login_redis(struct service* s) {
    struct redisproxy *self = SERVICE_SELF;
    const char* addr = sc_getstr("redis_ip", "");
    int port = sc_getint("redis_port", 0);
    sc_info("connect to redis %s:%u ...", addr, port);
    int err;
    int id = sc_net_block_connect(addr, port, s->serviceid, CLI_REDIS, &err);
    if (id < 0) {
        sc_error("connect to redis fail: %s", sc_net_error(err)); 
        return 1;
    }
    self->connid = id;
    const char* auth = sc_getstr("redis_auth", "123456");
    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp), "auth %s\r\n", auth);
    if (sc_net_block_send(id, tmp, len, &err) != len) {
        sc_error("auth fail: %s", sc_net_error(err)); 
        return 1;
    }
    sc_info("send ok");
    struct redis_reply reply;
    redis_initreply(&reply, 512, 16*1024);
    _redis_read(id, &reply);

    if (!redis_to_status(REDIS_ITEM(&reply))) {
        char tmp[1024];
        redis_to_string(REDIS_ITEM(&reply), tmp, sizeof(tmp));
        sc_error("redis auth error: %s", tmp);
        return 1;
    }
    redis_finireply(&reply);
    sc_info("login redis");
    return 0;
}

int
redisproxy_init(struct service* s) {
    struct redisproxy* self = SERVICE_SELF;
    self->connid = -1;
    
    //if (_connect_redis(s, true)) {
    if (_login_redis(s)) {
        return 1;
    } 
    redis_initreply(&self->reply, 512, 16*1024);
    FREELIST_INIT(&self->queryq);

    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISQUERY);
    sc_timer_register(s->serviceid, 1000);
    return 0;
}

static inline void
_query_to_redis(struct redisproxy* self, char* ptr, int sz) {
    if (self->connid != -1) {
        sc_net_send(self->connid, ptr, sz);
    } else {
        ptr[sz-1] = '\0';
        sc_rec(ptr);
    }
}

static void
_query(struct redisproxy* self, int id, struct UM_BASE* um) {
    UM_CAST(UM_REDISQUERY, rq, um);
    int datasz = (int)rq->msgsz - (int)sizeof(*rq) - (int)rq->cbsz;
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
        ql->nodeid = rq->nodeid;
        ql->needreply = rq->needreply;
        ql->cbsz = cbsz;
        if (cbsz > 0) {
            memcpy(ql->cb, cbptr, cbsz);
        }
    }
    if (self->connid != -1) {
        sc_net_send(self->connid, dataptr, datasz);
    } else {
        if (rq->needrecord) {
            dataptr[datasz-1] = '\0';
            sc_rec(dataptr);
        }
    }
}

void
redisproxy_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct redisproxy* self = SERVICE_SELF;
    UM_CAST(UM_BASE, um, msg);
    switch (um->msgid) {
    case IDUM_REDISQUERY:
        _query(self, id, um);
        break;
    }
}

static void
_handlereply(struct redisproxy* self) {
    //redis_walkreply(&self->reply); // todo: delete

    struct querylink* ql = FREELIST_POP(querylink, &self->queryq);
    assert(ql);
    if (ql->needreply == 0) {
        return; // no need reply
    }
    const struct sc_node* node = sc_node_get(ql->nodeid);
    if (node == NULL) {
        return; // the node disconnect
    }
    UM_DEFVAR(UM_REDISREPLY, rep);
    rep->cbsz = ql->cbsz;
   
    struct redis_reader* reader = &self->reply.reader;
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));
    if (ql->cbsz) {
        memrw_write(&rw, ql->cb, ql->cbsz);
    }
    memrw_write(&rw, reader->buf, reader->pos);
    rep->msgsz = RW_CUR(&rw) + sizeof(*rep);
    UM_SENDTONODE(node, rep, rep->msgsz);
}

static void
_read(struct redisproxy* self, struct net_message* nm) {
    assert(nm->ut == CLI_REDIS);
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
        int nread = sc_net_readto(id, buf, space, &e);
        if (nread <= 0) {
            goto errout;
        }
        reply->reader.sz += nread;
        int result = redis_getreply(reply);
        int K = 0;
        while (result == REDIS_SUCCEED) {
            _handlereply(self);
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
        sc_net_close_socket(id, true);
        nm->type = NETE_SOCKERR;
        nm->error = e;
        service_notify_net(nm->ud, nm);
    }
}

//static int
//_auth(struct redisproxy* self) {
    //return 1;    
//}

void
redisproxy_net(struct service* s, struct net_message* nm) {
    struct redisproxy* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm);
        break;
    case NETE_CONNECT:
        self->connid = nm->connid;
        sc_net_subscribe(nm->connid, true);
        sc_info("connect to redis ok");
        break;
    case NETE_CONNERR:
        self->connid = -1;
        FREELIST_POPALL(querylink, &self->queryq);
        sc_error("connect to redis fail: %s", sc_net_error(nm->error)); 
        break;
    case NETE_SOCKERR:
        self->connid = -1;
        FREELIST_POPALL(querylink, &self->queryq);
        sc_error("redis disconnect: %s", sc_net_error(nm->error));
        break;
    }
}

void
redisproxy_time(struct service* s) {
    struct redisproxy* self = SERVICE_SELF;
    if (self->connid == -1) {
        _connect_redis(s, false);
    }
    if (self->times > 0) {
        //sc_info("maxcount = %d, agvcount = %d", self->maxcount, self->allcount/self->times);
    }
}
