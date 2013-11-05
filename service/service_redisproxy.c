#include "host_service.h"
#include "host.h"
#include "host_dispatcher.h"
#include "host_timer.h"
#include "host_log.h"
#include "host_net.h"
#include "user_message.h"
#include "client_type.h"
#include "redis.h"
#include "freelist.h"
#include "memrw.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

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
    struct redis_reply reply;
    struct queryqueue queryq;
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
_connect_redis(struct service* s) {
    const char* addr = host_getstr("redis_ip", "");
    int port = host_getint("redis_port", 0);
    host_info("connect to redis %s:%u ...", addr, port);
    if (host_net_connect(addr, port, true, s->serviceid, CLI_REDIS)) { 
        return 1;
    }
    return 0;
}

int
redisproxy_init(struct service* s) {
    struct redisproxy* self = SERVICE_SELF;
    self->connid = -1;
    
    if (_connect_redis(s)) {
        return 1;
    } 
    redis_initreply(&self->reply, 512, 16*1024);
    FREELIST_INIT(&self->queryq);

    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISQUERY);
    host_timer_register(s->serviceid, 1000);
    return 0;
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
    host_net_send(self->connid, dataptr, datasz);
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
    const struct host_node* node = host_node_get(ql->nodeid);
    if (node == NULL) {
        return; // the node disconnect
    }
    UM_DEFVAR(UM_REDISREPLY, rep);
    rep->cbsz = ql->cbsz;
   
    struct redis_reader* reader = &self->reply.reader;
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));
    memrw_write(&rw, ql->cb, ql->cbsz);
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
        int nread = host_net_readto(id, buf, space, &e);
        if (nread <= 0) {
            goto errout;
        }
        reply->reader.sz += nread;
        int result = redis_getreply(reply);
        while (result == REDIS_SUCCEED) {
            _handlereply(self);
            redis_resetreply(reply); 
            result = redis_getreply(reply);
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
        host_net_close_socket(id);
        nm->type = NETE_SOCKERR;
        nm->error = e;
        service_notify_net(nm->ud, nm);
    }
}

void
redisproxy_net(struct service* s, struct net_message* nm) {
    struct redisproxy* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        _read(self, nm);
        break;
    case NETE_CONNECT:
        self->connid = nm->connid;
        host_net_subscribe(nm->connid, true);
        host_info("connect to redis ok");
        break;
    case NETE_CONNERR:
        self->connid = -1;
        FREELIST_POPALL(querylink, &self->queryq);
        host_error("connect to redis fail: %s", host_net_error(nm->error));
        break;
    case NETE_SOCKERR:
        self->connid = -1;
        FREELIST_POPALL(querylink, &self->queryq);
        host_error("redis disconnect: %s", host_net_error(nm->error));
        break;
    }
}

void
redisproxy_time(struct service* s) {
    /*
    struct redisproxy* self= SERVICE_SELF;
    if (self->connid >= 0) {
        char* tmp;
        //tmp = "*2\r\n$7\r\nhgetall\r\n$6\r\nuser:1\r\n";
        //host_net_send(self->connid, tmp, strlen(tmp));
        tmp = "hgetall user:1\r\n";
        host_net_send(self->connid, tmp, strlen(tmp));

    }
    */
}
