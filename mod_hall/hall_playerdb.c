#include "hall_playerdb.h"
#include "hall.h"
#include "sc_service.h"
#include "sh_util.h"
#include "sc.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_timer.h"
#include "redis.h"
#include "msg_server.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "memrw.h"
#include "hall_role.h"
#include "hall_ring.h"
#include "hall_attribute.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int
hall_playerdb_init(struct hall *self) {
    redis_initreply(&self->reply, 512, 0);
    return 0;
}

void
hall_playerdb_fini(struct hall *self) {
    redis_finireply(&self->reply);
}

static int
_db(struct service *s, struct player* p, int8_t type) {
    struct hall *self = SERVICE_SELF;

    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;
    uint32_t accid = cdata->accid;

    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->needreply = 0;
    rq->needrecord = 0;
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &type, sizeof(type)); 
    memrw_write(&rw, &accid, sizeof(accid));
    rq->cbsz = RW_CUR(&rw);
    switch (type) {
    case PDB_QUERY: {
        rq->needreply = 1;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "get acc:%u:user\r\n", accid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CHECKNAME: {
        rq->needreply = 1;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "get user:%s:name\r\n", cdata->name);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_SAVENAME: {
        rq->needreply = 1;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "set user:%s:name 1\r\n", cdata->name);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CHARID: {
        rq->needreply = 1;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "incr user:id\r\n");
        memrw_pos(&rw, len);
        }
        break;
    case PDB_LOAD: {
        rq->needreply = 1;
        uint32_t charid = cdata->charid;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmget user:%u"
                " name"
                " level"
                " exp"
                " coin"
                " diamond"
                " package"
                " role"
                " skin"
                " score1"
                " score2"
                " ownrole"
                " usepage"
                " npage"
                " pages"
                " nring"
                " rings"
                "\r\n", charid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_CREATE: {
        rq->needreply = 1;
        uint32_t charid = cdata->charid;
        cdata->coin = 1000000; // todo
        cdata->diamond = 100000; // todo
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
        uint32_t charid = cdata->charid;
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "set acc:%u:user %u\r\n",
                accid, charid);
        memrw_pos(&rw, len);
        }
        break;
    case PDB_SAVE: {
        rq->needreply = 0;
        rq->needrecord = 1;
        uint32_t charid = cdata->charid;

        char strownrole[sc_bytestr_encode_leastn(sizeof(cdata->ownrole))];
        sc_bytestr_encode((uint8_t*)cdata->ownrole, sizeof(cdata->ownrole), 
                          strownrole, sizeof(strownrole));
        char strpages[sc_bytestr_encode_leastn(sizeof(rdata->pages))];
        sc_bytestr_encode((uint8_t*)rdata->pages, min(rdata->npage, sizeof(rdata->pages)), 
                          strpages, sizeof(strpages));
        char strrings[sc_bytestr_encode_leastn(sizeof(rdata->rings))];
        sc_bytestr_encode((uint8_t*)rdata->rings, min(rdata->nring, sizeof(rdata->rings)), 
                          strrings, sizeof(strrings));
        int len = snprintf(rw.ptr, RW_SPACE(&rw), "hmset user:%u"
                " level %u"
                " exp %u"
                " coin %u"
                " diamond %u"
                " package %u"
                " role %u"
                " skin %u"
                " score1 %u"
                " score2 %u"
                " ownrole %s"
                " usepage %u"
                " npage %u"
                " pages %s"
                " nring %u"
                " rings %s"
                "\r\n", charid,
                cdata->level, 
                cdata->exp, 
                cdata->coin, 
                cdata->diamond, 
                cdata->package,
                cdata->role,
                cdata->skin,
                cdata->score_normal,
                cdata->score_dashi,
                strownrole,
                rdata->usepage,
                rdata->npage,
                strpages,
                rdata->nring,
                "strrings" // todo
                );
        memrw_pos(&rw, len);
        }
        break;
    default:
        return 1;
    }
    return sh_service_send(SERVICE_ID, self->rpuser_handle, MT_UM, rq, sizeof(*rq) + RW_CUR(&rw)); 
}

int 
hall_playerdb_send(struct service *s, struct player *pr, int type) {
    return _db(s, pr, type);
}


static int
_loadpdb(struct player* p, struct redis_replyitem* item) {
#define CHECK(x) if (si < end) {x; } else { return SERR_OK; }
    
    struct chardata* cdata = &p->data;
    uint32_t charid = cdata->charid;
    uint32_t accid  = cdata->accid;
    memset(cdata, 0, sizeof(*cdata));
    cdata->charid = charid;
    cdata->accid = accid;

    struct ringdata* rdata = &cdata->ringdata;
    struct redis_replyitem* si = item->child;
    struct redis_replyitem* end = si + item->value.i;
    if (redis_bulkitem_isnull(si)) {
        return SERR_DBDATAERR; // maybe no char, this is a empty item, all value is "-1"
    }
    int l = min(sizeof(cdata->name)-1, si->value.len);
    memcpy(cdata->name, si->value.p, l);
    cdata->name[l] = '\0';
    si++;
    CHECK(cdata->level = redis_bulkitem_toul(si++));
    CHECK(cdata->exp = redis_bulkitem_toul(si++));
    CHECK(cdata->coin = redis_bulkitem_toul(si++));
    CHECK(cdata->diamond = redis_bulkitem_toul(si++));
    CHECK(cdata->package = redis_bulkitem_toul(si++));
    CHECK(cdata->role = redis_bulkitem_toul(si++));
    CHECK(cdata->skin = redis_bulkitem_toul(si++));
    CHECK(cdata->score_normal = redis_bulkitem_toul(si++));
    CHECK(cdata->score_dashi = redis_bulkitem_toul(si++));
    CHECK(
    sc_bytestr_decode(si->value.p, si->value.len, (uint8_t*)cdata->ownrole, sizeof(cdata->ownrole));
    si++;)
    CHECK(rdata->usepage = redis_bulkitem_toul(si++));
    CHECK(rdata->npage = redis_bulkitem_toul(si++));
    CHECK(
    sc_bytestr_decode(si->value.p, si->value.len, (uint8_t*)rdata->pages, sizeof(rdata->pages));
    si++;
    );
    CHECK(rdata->nring = redis_bulkitem_toul(si++));
    CHECK(
    sc_bytestr_decode(si->value.p, si->value.len, (uint8_t*)rdata->rings, sizeof(rdata->rings));
    si++;
    );
    return SERR_OK;
}

void 
hall_playerdb_process_redis(struct service *s, struct UM_REDISREPLY *rep, int sz) {
    struct hall *self = SERVICE_SELF;

    int8_t type = 0; 
    uint32_t accid = 0;
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));
    memrw_read(&rw, &type, sizeof(type));
    memrw_read(&rw, &accid, sizeof(accid));

    struct player* p = sh_hash_find(&self->acc2player, accid);
    if (p == NULL) {
        return; // maybe disconnect
    }

    int32_t serr = SERR_UNKNOW;
    switch (type) {
    case PDB_QUERY: {
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
        //if (_hashplayer(p, charid)) {
            //serr = SERR_WORLDFULL;
            //break;
        //}
        p->data.charid = charid;
        p->status = PS_LOADCHAR;
        _db(s, p, PDB_LOAD);
        return;
        }
        break;
    case PDB_CHECKNAME: {
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
        _db(s, p, PDB_SAVENAME);
        return;
        }
        break;
    case PDB_SAVENAME: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_STATUS) {
            p->status = PS_CHARUNIQUEID;
            _db(s, p, PDB_CHARID);
            return;
        } else if (item->type == REDIS_REPLY_ERROR) {
            serr = SERR_DBERR;
        } else {
            serr = SERR_DBREPLYTYPE;
        }
        }
        break;
    case PDB_CHARID: {
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
        //if (_hashplayer(p, charid)) {
            //serr = SERR_WORLDFULL;
            //break;
        //}
        p->data.charid = charid;
        p->status = PS_CREATECHAR;
        _db(s, p, PDB_CREATE);
        return;
        }
        break;
    case PDB_CREATE: {
        redis_resetreplybuf(&self->reply, rw.ptr, RW_SPACE(&rw));
        if (redis_getreply(&self->reply) != REDIS_SUCCEED) {
            serr = SERR_DBREPLY;
            break;
        }
        struct redis_replyitem* item = self->reply.stack[0];
        if (item->type == REDIS_REPLY_STATUS) {
            p->status = PS_BINDCHARID;
            _db(s, p, PDB_BINDCHARID);
            return;
        } else if (item->type == REDIS_REPLY_ERROR) {
            serr = SERR_DBERR;
        } else {
            serr = SERR_DBREPLYTYPE;
        }
        }
        break;
    case PDB_BINDCHARID: {
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
    switch (serr) {
    case SERR_OK:
        if (p->status == PS_LOGIN) {
            p->status = PS_HALL;
            
            UM_DEFFIX(UM_ENTERHALL, enter);
            hall_role_main(s, p, enter, sizeof(*enter));
            hall_ring_main(s, p, enter, sizeof(*enter));
            hall_attribute_main(self->T, &p->data); 
            
            hall_sync_role(s, p);
        }
        break;
    case SERR_NOCHAR:
    case SERR_NAMEEXIST:
        hall_notify_login_fail(s, p, serr);
        break;
    default: {
        UM_DEFFIX(UM_LOGOUT, lo);
        hall_notify_logout(s, p, serr);
        hall_player_main(s, 0, p, lo, sizeof(*lo));
        break;
        }
    }
}
