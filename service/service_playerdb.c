#include "sc_service.h"
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
#include "playerdb.h"
#include "sharetype.h"
#include "memrw.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct playerdb {
    uint32_t requester;
    struct redis_reply reply;
};

struct playerdb*
playerdb_create() {
    struct playerdb* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
playerdb_free(struct playerdb* self) {
    redis_finireply(&self->reply);
    free(self);
}

int
playerdb_init(struct service* s) {
    struct playerdb* self = SERVICE_SELF;
    self->requester = service_query_id("world");
    if (self->requester == SERVICE_INVALID) {
        sc_error("lost world service");
        return 1;
    }
    
    redis_initreply(&self->reply, 512, 0);
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    return 0;
}

static int
_db(struct player* p, int8_t type) {
    struct chardata* cdata = &p->data;

    UM_DEFVAR(UM_REDISQUERY, rq);
    struct memrw rw;
    memrw_init(&rw, rq->data, rq->msgsz - sizeof(*rq));
    memrw_write(&rw, &type, sizeof(type)); 
    memrw_write(&rw, &p->gid, sizeof(p->gid));
    memrw_write(&rw, &p->cid, sizeof(p->cid));
    switch (type) {
    case PDB_QUERY: {
        rq->needreply = 1;
        uint32_t accid = cdata->accid;
        memrw_write(&rw, &accid, sizeof(accid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "get user:%u:accid\r\n", accid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CHECKNAME: {
        rq->needreply = 1;
        uint32_t accid = cdata->accid;
        memrw_write(&rw, &accid, sizeof(accid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "get user:%s:name\r\n", cdata->name);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_SAVENAME: {
        rq->needreply = 1;
        uint32_t accid = cdata->accid;
        memrw_write(&rw, &accid, sizeof(accid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "set user:%s:name 1\r\n", cdata->name);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CHARID: {
        rq->needreply = 1;
        uint32_t accid = cdata->accid;
        memrw_write(&rw, &accid, sizeof(accid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "incr user:id\r\n");
        memrw_pos(&rw, len);
        }
        break;
    case PDB_LOAD: {
        rq->needreply = 1;
        uint32_t charid = cdata->charid;
        memrw_write(&rw, &charid, sizeof(charid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmget user:%u"
                " name"
                " level"
                " exp"
                " coin"
                " diamond"
                " package"
                " role"
                " skin"
                "\r\n", charid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CREATE: {
        rq->needreply = 1;
        uint32_t charid = cdata->charid;
        memrw_write(&rw, &charid, sizeof(charid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmset user:%u"
                " name %s"
                " level %u"
                " exp %u"
                " coin %u"
                " diamond %u"
                " package %u"
                " role %u"
                " skin %u"
                "\r\n", charid,
                cdata->name,
                cdata->level, 
                cdata->exp, 
                cdata->coin, 
                cdata->diamond, 
                cdata->package,
                cdata->role,
                cdata->skin);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_BINDCHARID: {
        rq->needreply = 1;
        uint32_t accid  = cdata->accid;
        uint32_t charid = cdata->charid;
        memrw_write(&rw, &charid, sizeof(charid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "set user:%u:accid %u\r\n",
                accid, charid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_SAVE: {
        rq->needreply = 0;
        uint32_t charid = cdata->charid;
        memrw_write(&rw, &charid, sizeof(charid));
        rq->cbsz = RW_CUR(&rw);
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmset user:%u"
                " name %s"
                " level %u"
                " exp %u"
                " coin %u"
                " diamond %u"
                " package %u"
                " role %u"
                " skin %u"
                "\r\n", charid,
                cdata->name,
                cdata->level, 
                cdata->exp, 
                cdata->coin, 
                cdata->diamond, 
                cdata->package,
                cdata->role,
                cdata->skin);
        memrw_pos(&rw, len);
        }
        break;
    default:
        return 1;
    }

    const struct sc_node* redisp = sc_node_get(HNODE_ID(NODE_REDISPROXY, 0));
    if (redisp == NULL) {
        return 1;
    }
    rq->msgsz = sizeof(*rq) + RW_CUR(&rw);
    UM_SENDTONODE(redisp, rq, rq->msgsz);
    return 0;
}

static int
_loadpdb(struct player* p, struct redis_replyitem* item) {
    if (item->value.i != 8)
        return SERR_DBDATAERR;
    struct chardata* cdata = &p->data;
    struct redis_replyitem* si = item->child;
    
    if (strncpychk(cdata->name, sizeof(cdata->name), si->value.p, si->value.len)) {
        return SERR_DBDATAERR; // maybe no char, this is a empty item, all value is "-1"
    }
    si++;
    cdata->level = redis_bulkitem_toul(si++);
    cdata->exp = redis_bulkitem_toul(si++);
    cdata->coin = redis_bulkitem_toul(si++);
    cdata->diamond = redis_bulkitem_toul(si++);
    cdata->package = redis_bulkitem_toul(si++);
    cdata->role = redis_bulkitem_toul(si++);
    cdata->skin = redis_bulkitem_toul(si++);
    return SERR_OK;
}

void
playerdb_service(struct service* s, struct service_message* sm) {
    //struct playerdb* self = SERVICE_SELF;
    assert(sm->sz == sizeof(struct playerdbcmd));
    struct playerdbcmd* cmd = sm->msg;
    cmd->err = _db(cmd->p, cmd->type);
}

static void
_dbresponse(struct playerdb* self, struct player* p, int error) {
    struct service_message sm;
    sm.sessionid = 0;
    sm.source = 0;
    struct playerdbres res;
    res.p = p;
    res.error = error;
    sm.msg = &res;
    sm.sz = sizeof(res);
    service_notify_service(self->requester, &sm);
};

static void
_handleredis(struct playerdb* self, struct node_message* nm) { 
    hassertlog(nm->um->msgid == IDUM_REDISREPLY);
    UM_CAST(UM_REDISREPLY, rep, nm->um);
        
    int8_t type = 0; 
    uint16_t gid = 0;
    uint16_t cid = 0;
    struct memrw rw;
    memrw_init(&rw, rep->data, rep->msgsz - sizeof(*rep));
    memrw_read(&rw, &type, sizeof(type));
    memrw_read(&rw, &gid, sizeof(gid));
    memrw_read(&rw, &cid, sizeof(cid));

    struct player* p = _getplayer(gid, cid);
    if (p == NULL) {
        return; // maybe disconnect
    }

    int32_t serr = SERR_UNKNOW;
    switch (type) {
    case PDB_QUERY: {
        uint32_t accid = 0;
        memrw_read(&rw, &accid, sizeof(accid));
        if (p->data.accid != accid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_STRING) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        uint32_t charid = redis_bulkitem_toul(item);
        if (charid == 0) {
            p->status = PS_WAITCREATECHAR;
            serr = SERR_NOCHAR;
            break;
        }
        if (_hashplayer(p, charid)) {
            serr = SERR_WORLDFULL;
            break;
        }
        p->status = PS_LOADCHAR;
        _db(p, PDB_LOAD);
        return;
        }
        break;
    case PDB_CHECKNAME: {
        uint32_t accid = 0;
        memrw_read(&rw, &accid, sizeof(accid));
        if (p->data.accid != accid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_STRING) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        if (!redis_bulkitem_isnull(item)) {
            p->status = PS_WAITCREATECHAR;
            // limit create char times
            p->createchar_times++;
            if (p->createchar_times >= 20) { 
                serr = SERR_CREATECHARMUCHTIMES;
            } else {
                serr = SERR_NAMEEXIST;
            }
            break;
        }
        p->status = PS_SAVECHARNAME;
        _db(p, PDB_SAVENAME);
        return;
        }
        break;
    case PDB_SAVENAME: {
        uint32_t accid = 0;
        memrw_read(&rw, &accid, sizeof(accid));
        if (p->data.accid != accid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_STATUS) {
            p->status = PS_CHARUNIQUEID;
            _db(p, PDB_CHARID);
            return;
        } else if (item->type == REDIS_REPLY_ERROR) {
            serr = SERR_DBERR;
        } else {
            serr = SERR_DBREPLYTYPE;
        }
        }
        break;
    case PDB_CHARID: {
        uint32_t accid = 0;
        memrw_read(&rw, &accid, sizeof(accid));
        if (p->data.accid != accid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];        
        if (item->type != REDIS_REPLY_INTEGER) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        uint32_t charid = (uint32_t)item->value.u;
        if (charid == 0) {
            serr = SERR_UNIQUECHARID;
            break;
        }
        if (_hashplayer(p, charid)) {
            serr = SERR_WORLDFULL;
            break;
        }
        p->status = PS_CREATECHAR;
        _db(p, PDB_CREATE);
        return;
        }
        break;
    case PDB_CREATE: {
        uint32_t charid = 0;
        memrw_read(&rw, &charid, sizeof(charid));
        if (p->data.charid != charid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_STATUS) {
            p->status = PS_BINDCHARID;
            _db(p, PDB_BINDCHARID);
            return;
        } else if (item->type == REDIS_REPLY_ERROR) {
            serr = SERR_DBERR;
        } else {
            serr = SERR_DBREPLYTYPE;
        }
        }
        break;
    case PDB_BINDCHARID: {
        uint32_t charid = 0;
        memrw_read(&rw, &charid, sizeof(charid));
        if (p->data.charid != charid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_STATUS) {
            p->status = PS_LOGIN;
            serr = SERR_OK;
        } else if (item->type == REDIS_REPLY_ERROR) {
            serr = SERR_DBERR;
        } else {
            serr = SERR_DBREPLYTYPE;
        }
        }
        break;
    case PDB_LOAD: {
        uint32_t charid = 0;
        memrw_read(&rw, &charid, sizeof(charid));
        if (p->data.charid != charid) {
            return; // other
        }
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type != REDIS_REPLY_ARRAY) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        serr = _loadpdb(p, item);
        if (serr == SERR_OK) {
            p->status = PS_LOGIN;
        }
        }
        break;
    default:
        return;
    }
    _dbresponse(self, p, serr);
}

void
playerdb_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct playerdb* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_REDISPROXY:
        _handleredis(self, &nm);
        break;
    }
}
