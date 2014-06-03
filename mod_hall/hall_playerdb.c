#include "hall_playerdb.h"
#include "hall.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "hall_role.h"
#include "hall_ring.h"
#include "hall_attribute.h"
#include "hall_washgold.h"
#include "sh.h"
#include "msg_server.h"
#include "redis.h"
#include "memrw.h"

int
hall_playerdb_init(struct hall *self) {
    redis_initreply(&self->reply, 512, 0);
    return 0;
}

void
hall_playerdb_fini(struct hall *self) {
    redis_finireply(&self->reply);
}

static void
char_create(struct chardata *cdata) {
    cdata->luck_factor = 0.5f; // todo
    cdata->coin = 1000000; // todo
    cdata->diamond = 100000; // todo
}

#define SEND_RP(handle) \
    sh_handle_send(MODULE_ID, handle, MT_UM, rq, sizeof(*rq) + RW_CUR(&rw))

int 
hall_player_first(struct module *s, struct player *p, int field_t) {
    struct hall *self = MODULE_SELF;
    static const char *FIRST_FIELDS[] = {
        "account",
        "create_char",
        "enter_hall",
        "play_game",
        "freedom_play",
        "freedom_over",
        "dashi_play",
        "dashi_match",
        "dashi_over",
        "role_use",
        "role_buy_click",
        "role_buy_ok",
        "setting_click",
        "help_click",
        "newbie_click",
        "c_click",
        "rank_click",
        "score_click",
        "dashi_click",
        "washgold",
        "adjuststate",
        "exchange",
        "return_ground",
    };
    if (field_t >= 0 && 
        field_t < sizeof(FIRST_FIELDS)/sizeof(FIRST_FIELDS[0])) {
        return 1;
    }
    struct chardata* cdata = &p->data;
    if (!((cdata->first_time_flag >> field_t) & 1)) {
        return 1;
    }
    const char *field = FIRST_FIELDS[field_t];
    uint32_t charid = cdata->charid;
    uint32_t accid = cdata->accid;
    int8_t type = PDB_FIRST;
    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &charid, sizeof(charid));
    memrw_write(&rw, &accid, sizeof(accid));
    memrw_write(&rw, &type, sizeof(type)); 
    rq->cbsz = RW_CUR(&rw);
    rq->flag = RQUERY_SHARDING;
    int len = redis_format(&rw.ptr, RW_SPACE(&rw), "hmset first:%u %s %u",
            charid, field, sh_timer_now()/1000);
    memrw_pos(&rw, len);
    int r = SEND_RP(self->rpuser_handle);
    if (!r) {
        cdata->first_time_flag |= 1<<type;
    }
    return r;
}

static int
_db(struct module *s, struct player* p, int8_t type) {
    struct hall *self = MODULE_SELF;

    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;
    uint32_t charid = cdata->charid;
    uint32_t accid = cdata->accid;

    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    struct memrw rw;
    memrw_init(&rw, rq->data, UM_MAXSZ - sizeof(*rq));
    memrw_write(&rw, &charid, sizeof(charid));
    memrw_write(&rw, &accid, sizeof(accid));
    memrw_write(&rw, &type, sizeof(type)); 
    rq->cbsz = RW_CUR(&rw);
    switch (type) {
    case PDB_QUERY: {
        rq->flag = RQUERY_REPLY;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "get acc:%u:user", accid);
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuseruni_handle);
        }
    case PDB_CHECKNAME: {
        rq->flag = RQUERY_REPLY;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "setnx user:%s:name %u", cdata->name, charid);
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuseruni_handle);
        }
    case PDB_CHARID: {
        rq->flag = RQUERY_REPLY;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "incr user:id");
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuseruni_handle);
        }
    case PDB_BINDCHARID: {
        rq->flag = RQUERY_REPLY;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "set acc:%u:user %u",
                accid, charid);
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuseruni_handle);
        }
    case PDB_LOAD: {
        rq->flag = RQUERY_REPLY|RQUERY_SHARDING;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "hmget user:%u"
                " name"
                " level"
                " exp"
                " coin"
                " diamond"
                " package"
                " role"
                " luck_factor"
                " last_washgold_refresh_time"
                " washgold"
                " last_state_refresh_time"
                " score1"
                " score2"
                " ownrole"
                " usepage"
                " npage"
                " pages"
                " nring"
                " rings"
                " states"
                " first_time_flag"
                " depth_max" // 最高深度
                " time_max"  // 最大单局时长
                " speed_max" // 最高速率
                " score_max" // 最高分数
                " coin_max"  // 最多货币
                " bao_item_max"   // 最多宝物
                " game_times_acc" // 总游戏局数
                " depth_acc" // 总游戏深度
                " online_time_acc"// 总游戏时长
                , charid);
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuser_handle);
        }
    case PDB_CREATE: {
        rq->flag = RQUERY_REPLY|RQUERY_SHARDING;
        char_create(cdata);
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "hmset user:%u"
                " name %s"
                " accid %u"
                " level %u"
                " exp %u"
                " coin %u"
                " diamond %u"
                " package %u"
                " role %u", charid,
                cdata->name,
                accid,
                cdata->level, 
                cdata->exp, 
                cdata->coin, 
                cdata->diamond, 
                cdata->package,
                cdata->role);
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuser_handle);
        }
    case PDB_SAVE: {
        rq->flag = RQUERY_BACKUP|RQUERY_SHARDING;
        uint32_t charid = cdata->charid;
        int len = redis_format(&rw.ptr, RW_SPACE(&rw), "hmset user:%u"
                " level %u"
                " exp %u"
                " coin %u"
                " diamond %u"
                " package %u"
                " role %u"
                " luck_factor %f"
                " last_washgold_refresh_time %u"
                " washgold %u"
                " last_state_refresh_time %u"
                " score1 %u"
                " score2 %u"
                " usepage %u"
                " npage %u"
                " nring %u"
                " ownrole %b"
                " pages %b"
                " rings %b"
                " states %b"
                " first_time_flag %u"
                " depth_max %u" // 最高深度
                " time_max %u"  // 最大单局时长
                " speed_max %u" // 最高速率
                " score_max %u" // 最高分数
                " coin_max %u"  // 最多货币
                " bao_item_max %u"   // 最多宝物
                " game_times_acc %u"// 总游戏局数
                " depth_acc %u"// 总游戏深度
                " online_time_acc %u" // 总游戏时长
                ,
                charid,
                cdata->level, 
                cdata->exp, 
                cdata->coin, 
                cdata->diamond, 
                cdata->package,
                cdata->role,
                cdata->luck_factor,
                cdata->last_washgold_refresh_time,
                cdata->washgold,
                cdata->last_state_refresh_time,
                cdata->score_normal,
                cdata->score_dashi,
                rdata->usepage,
                rdata->npage,
                rdata->nring,
                cdata->ownrole, sizeof(cdata->ownrole),
                rdata->pages, rdata->npage,
                rdata->rings, rdata->nring,
                cdata->roles_state, sizeof(cdata->roles_state),
                cdata->first_time_flag,
                p->stat_max[ST_depth], // 最高深度
                p->stat_max[ST_time],  // 最大单局时长
                p->stat_max[ST_speed], // 最高速率
                p->stat_max[ST_score], // 最高分数
                p->stat_max[ST_coin],  // 最多货币
                p->stat_max[ST_bao_item],   // 最多宝物
                p->stat_acc[ST_game_times], // 总游戏局数
                p->stat_acc[ST_depth],      // 总游戏深度
                p->stat_acc[ST_online_time] // 总游戏时长
                );
        if (len == 0) {
            sh_error("PDB_SAVE error, %u", charid);
            return 1;
        }
        memrw_pos(&rw, len);
        return SEND_RP(self->rpuser_handle);
        }
    default:
        return 1;
    }
}

int 
hall_playerdb_send(struct module *s, struct player *pr, int type) {
    if (type != PDB_SAVE) {
        return _db(s, pr, type);
    } else {
        return hall_playerdb_save(s, pr, true);
    }
}

int
hall_playerdb_save(struct module *s, struct player *pr, bool force) {
    uint64_t now = sh_timer_now();
    if (force || 
        now - pr->last_save_time >= 1000 * PDB_SAVE_INTV) {
        pr->stat_acc[ST_online_time] += now - pr->last_online_time;
        if (_db(s, pr, PDB_SAVE)) {

            return 1; 
        }
        pr->last_save_time = now;
        pr->last_online_time = now;
    }
    return 0;
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
    CHECK(cdata->luck_factor = redis_bulkitem_tof(si++));
    CHECK(cdata->last_washgold_refresh_time = redis_bulkitem_toul(si++)); 
    CHECK(cdata->washgold = redis_bulkitem_toul(si++));
    CHECK(cdata->last_state_refresh_time = redis_bulkitem_toul(si++));
    CHECK(cdata->score_normal = redis_bulkitem_toul(si++));
    CHECK(cdata->score_dashi = redis_bulkitem_toul(si++));
    CHECK(
    memcpy(cdata->ownrole, si->value.p, min(sizeof(cdata->ownrole), si->value.len));
    si++;)
    CHECK(rdata->usepage = redis_bulkitem_toul(si++));
    CHECK(rdata->npage = redis_bulkitem_toul(si++));
    CHECK(
    memcpy(rdata->pages, si->value.p, min(sizeof(rdata->pages), si->value.len));
    si++;
    );
    CHECK(rdata->nring = redis_bulkitem_toul(si++));
    CHECK(
    memcpy(rdata->rings, si->value.p, min(sizeof(rdata->rings), si->value.len));
    si++;
    );
    CHECK(
    memcpy(cdata->roles_state, si->value.p, min(sizeof(cdata->roles_state), si->value.len));
    si++;)
    CHECK(cdata->first_time_flag = redis_bulkitem_toul(si++));
    CHECK(p->stat_max[ST_depth] = redis_bulkitem_toul(si++)); // 最高深度
    CHECK(p->stat_max[ST_time] = redis_bulkitem_toul(si++));  // 最大单局时长
    CHECK(p->stat_max[ST_speed] = redis_bulkitem_toul(si++)); // 最高速率
    CHECK(p->stat_max[ST_score] = redis_bulkitem_toul(si++)); // 最高分数
    CHECK(p->stat_max[ST_coin] = redis_bulkitem_toul(si++));  // 最多货币
    CHECK(p->stat_max[ST_bao_item] = redis_bulkitem_toul(si++));   // 最多宝物
    CHECK(p->stat_acc[ST_game_times] = redis_bulkitem_toul(si++));// 总游戏局数
    CHECK(p->stat_acc[ST_depth] = redis_bulkitem_toul(si++));// 总游戏深度
    CHECK(p->stat_acc[ST_online_time] = redis_bulkitem_toul(si++)); // 总游戏时长
    return SERR_OK;
}

void 
hall_playerdb_process_redis(struct module *s, struct UM_REDISREPLY *rep, int sz) {
    struct hall *self = MODULE_SELF;

    int8_t type = 0; 
    uint32_t charid = 0;
    uint32_t accid = 0;
    struct memrw rw;
    memrw_init(&rw, rep->data, sz - sizeof(*rep));
    memrw_read(&rw, &charid, sizeof(charid));
    memrw_read(&rw, &accid, sizeof(accid));
    memrw_read(&rw, &type, sizeof(type));

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
            p->status = PS_CHARUNIQUEID;
            _db(s, p, PDB_CHARID);
            return;
        }
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
        if (item->type != REDIS_REPLY_INTEGER) {
            serr = SERR_DBREPLYTYPE;
            break;
        }
        int r = (uint32_t)item->value.u;
        if (r == 1) { 
            p->status = PS_CREATECHAR;
            _db(s, p, PDB_CREATE);
            hall_player_first(s, p, FT_CREATE_CHAR);
            return;
        } else {
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
        p->data.charid = charid;
        p->status = PS_WAITCREATECHAR;
        serr = SERR_NOCHAR;
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
            p->last_online_time = sh_timer_now();
 
            UM_DEFFIX(UM_ENTERHALL, enter);
            enter->uid = UID(p);
            enter->ip[0] = '\0';
            hall_role_main(s, p, enter, sizeof(*enter));
            hall_ring_main(s, p, enter, sizeof(*enter));
            // before attribute refresh
            hall_attribute_main(self->T, &p->data); 
            // after attribute refresh
            hall_washgold_main(s, p, enter, sizeof(*enter)); 
            hall_sync_role(s, p);
            hall_player_first(s, p, FT_ENTER_HALL);
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
