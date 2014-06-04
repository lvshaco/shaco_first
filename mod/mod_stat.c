#include "sh.h"
#include "redis.h"
#include "msg_server.h"
#include "memrw.h"
#include <time.h>

#define DAY_SEC (24*3600)
#define DAY_OFF(t) ((t).tm_hour*3600 + (t).tm_min*60 + (t).tm_sec)

struct user {
    uint32_t id;
    uint32_t daily_max[ST_max];
    uint32_t daily_acc[ST_max];
    uint32_t week_max[ST_max];
    uint32_t week_acc[ST_max];
};

struct stat {
    int rpstat_handle;
    int daily_db;
    int week_db;
    time_t base_backup_time;
    time_t next_daily_backup;
    time_t next_week_backup;
    struct sh_hash users;
    struct redis_reply reply;
};

struct stat*
stat_create() {
    struct stat* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
stat_free(struct stat* self) {
    sh_hash_fini(&self->users);
    redis_finireply(&self->reply);
    free(self);
}

static void
next_daily_backup(struct stat *self) {
    time_t ni = sh_timer_now()/1000;
    struct tm t = *localtime(&ni);
    time_t base = ni - DAY_OFF(t);
    self->next_daily_backup = base + DAY_SEC;

    self->daily_db = (ni - self->base_backup_time) / DAY_SEC;
    self->daily_db &= 1;
    self->daily_db += 1;
}

static void
next_week_backup(struct stat *self) {
    time_t ni = sh_timer_now()/1000;
    struct tm t = *localtime(&ni);
    if (t.tm_wday == 0) {
        t.tm_wday = 7;
    }
    t.tm_wday--;
    time_t base = ni- (t.tm_wday * DAY_SEC) - DAY_OFF(t);
    self->next_week_backup = base + DAY_SEC * 7;

    self->week_db = (ni - self->base_backup_time) / (DAY_SEC * 7);
    self->week_db &= 1;
    self->week_db += 3;
}

int
stat_init(struct module* s) {
    struct stat* self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handle_subscribe("rpstat", SUB_REMOTE, &self->rpstat_handle)) {
        return 1;
    }
    sh_hash_init(&self->users, 1);
    redis_initreply(&self->reply, 512, 0);

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 2000-1900;
    t.tm_mon = 0;
    t.tm_mday = 1;
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    self->base_backup_time = mktime(&t);
    
    next_daily_backup(self);
    next_week_backup(self);

    sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static int
rd(struct module *s, int db, const char *cmd, int len) {
    struct stat *self = MODULE_SELF;
    UM_DEFVAR2(UM_REDISQUERY, rq, UM_MAXSZ);
    rq->flag = RQUERY_SHARDING;
    rq->cbsz = 4;
    assert(len < UM_MAXSZ - sizeof(*rq));
    *((uint32_t*)rq->data) = db;
    memcpy(rq->data+rq->cbsz, cmd, len);
    int msgsz = sizeof(*rq) + rq->cbsz + len;
    return sh_handle_send(MODULE_ID, self->rpstat_handle, MT_UM, rq, msgsz);
}

static void
rd_time(struct module *s, int db) {
    time_t now = sh_timer_now()/1000;
    char strtime[32];
    struct tm t = *localtime(&now);
    strftime(strtime, sizeof(strtime), "%y%m%d-%H:%M:%S", &t);

    char buf[1024];
    char *cmd = buf;
    int len = redis_format(&cmd, sizeof(buf), "SET btime %s", strtime);
    rd(s, db, cmd, len);
}

static void
rd_daily_stat(struct module *s, struct user *ur) {
    struct stat *self = MODULE_SELF;
    char buf[1024];
    char *cmd = buf;
    int len = redis_format(&cmd, sizeof(buf), "HMSET daily_stat:%u"
            " depth_max %u"
            " time_max %u"
            " score_max %u"
            " depth_acc %u"
            " coin_acc %u"
            " oxygen_item_acc %u"
            " fight_item_acc %u"
            " bao_item_acc %u"
            " co_times_acc %u"
            " dashi_times_acc %u"
            " dashi_wins_acc %u"
            " dashi_fails_acc %u"
            " game_times_acc %u"
            " time_acc %u"
            " online_time_acc %u",
            ur->id,
            ur->daily_max[ST_depth],
            ur->daily_max[ST_time],
            ur->daily_max[ST_score],
            ur->daily_acc[ST_depth], 
            ur->daily_acc[ST_coin], 
            ur->daily_acc[ST_oxygen_item], 
            ur->daily_acc[ST_fight_item], 
            ur->daily_acc[ST_bao_item], 
            ur->daily_acc[ST_co_times], 
            ur->daily_acc[ST_dashi_times], 
            ur->daily_acc[ST_dashi_wins], 
            ur->daily_acc[ST_dashi_fails], 
            ur->daily_acc[ST_game_times], 
            ur->daily_acc[ST_time], 
            ur->daily_acc[ST_online_time]); 
    rd(s, self->daily_db, cmd, len); 
}

static void
rd_week_stat(struct module *s, struct user *ur) {
    struct stat *self = MODULE_SELF;
    char buf[1024];
    char *cmd = buf;
    int len = redis_format(&cmd, sizeof(buf), "HMSET week_stat:%u"
            " depth_max %u"
            " time_max %u"
            " score_max %u"
            " depth_acc %u"
            " coin_acc %u"
            " oxygen_item_acc %u"
            " fight_item_acc %u"
            " bao_item_acc %u"
            " co_times_acc %u"
            " dashi_times_acc %u"
            " dashi_wins_acc %u"
            " dashi_fails_acc %u"
            " game_times_acc %u"
            " time_acc %u"
            " online_time_acc %u",
            ur->id,
            ur->week_max[ST_depth],
            ur->week_max[ST_time],
            ur->week_max[ST_score],
            ur->week_acc[ST_depth], 
            ur->week_acc[ST_coin], 
            ur->week_acc[ST_oxygen_item], 
            ur->week_acc[ST_fight_item], 
            ur->week_acc[ST_bao_item], 
            ur->week_acc[ST_co_times], 
            ur->week_acc[ST_dashi_times], 
            ur->week_acc[ST_dashi_wins], 
            ur->week_acc[ST_dashi_fails], 
            ur->week_acc[ST_game_times], 
            ur->week_acc[ST_time], 
            ur->week_acc[ST_online_time]); 
    rd(s, self->week_db, cmd, len);
}

static struct user *
login(struct module *s, uint32_t id) {
    struct stat *self = MODULE_SELF;
    struct user *ur = sh_hash_find(&self->users, id);
    if (ur == NULL) {
        ur = malloc(sizeof(*ur));
        memset(ur, 0, sizeof(*ur));
        ur->id = id;
        assert(!sh_hash_insert(&self->users, id, ur));
    }
    return ur;
}

static void
logout(struct module *s, uint32_t id) {
    struct stat *self = MODULE_SELF;
    struct user *ur = sh_hash_remove(&self->users, id);
    if (ur) {
        free(ur);
    } 
}

static void
handle_st(struct module *s, uint32_t id, uint32_t flag, uint32_t *data) {
    struct user *ur = login(s, id);
    assert(ur);
    bool dirty_daily = false;
    bool dirty_week = false;
    uint32_t *p = data;
    int i;
    for (i=0; i<ST_max; ++i) {
        if ((flag>>i) & 1) {
            if ((ST_DAILY_MAX>>i) & 1) {
                if (ur->daily_max[i] < *p) {
                    ur->daily_max[i] = *p;
                    dirty_daily = true;
                }
            }
            if ((ST_DAILY_ACC>>i) & 1) {
                ur->daily_acc[i] += *p;
                dirty_daily = true;
            }
            if ((ST_WEEK_MAX>>i) & 1) {
                if (ur->week_max[i] < *p) {
                    ur->week_max[i] = *p;
                    dirty_week = true;
                }
            }
            if ((ST_WEEK_ACC>>i) & 1) {
                ur->week_acc[i] += *p;
                dirty_week = true;
            }
            p++;
        }
    }
    if (dirty_daily) {
        rd_daily_stat(s, ur);
    }
    if (dirty_week) {
        rd_week_stat(s, ur);
    }
}

void
stat_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_STAT: {
            UM_CAST(UM_STAT, st, msg);
            handle_st(s, st->id, st->flag, st->data);
            break;
            }
        case IDUM_STATEND: {
            UM_CAST(UM_STATEND, se, msg);
            logout(s, se->id);
            break;
            }
        }
        break;
        }
    }
}

static void
daily_reset(void *pointer) {
    struct user *ur = pointer;
    memset(ur->daily_max, 0, sizeof(ur->daily_max));
    memset(ur->daily_acc, 0, sizeof(ur->daily_acc));
}

static void
week_reset(void *pointer) {
    struct user *ur = pointer;
    memset(ur->week_max, 0, sizeof(ur->week_max));
    memset(ur->week_acc, 0, sizeof(ur->week_acc));
}

void
stat_time(struct module *s) {
    struct stat *self = MODULE_SELF;

    time_t now = sh_timer_now()/1000;
    if (self->next_daily_backup <= now) {
        sh_hash_foreach(&self->users, daily_reset);
        next_daily_backup(self); 
        rd_time(s, self->daily_db);
    }
    if (self->next_week_backup <= now) {
        sh_hash_foreach(&self->users, week_reset);
        next_week_backup(self);
        rd_time(s, self->week_db);
    }
}
