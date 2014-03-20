#include "hall.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "hall_luck.h"
#include "msg_server.h"
#include "msg_client.h"
#include "sh.h"

static inline void
sync_washgold_info(struct module *s, struct player *pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_WASHGOLD_INFO, wi);
    cl->uid  = UID(pr);
    wi->washgold = pr->data.washgold; 
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*wi));

}

static inline void
sync_washgold_result(struct module *s, struct player *pr, uint8_t gain, uint8_t extra) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_WASHGOLD_RES, wr);
    cl->uid  = UID(pr);
    wr->gain = gain;
    wr->extra_gain = extra;
    wr->washgold = pr->data.washgold; 
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*wr));
}

static inline void
refresh_washgold(struct module *s, struct player *pr, bool sync) {
    struct chardata* cdata = &pr->data;
    uint32_t now = sh_timer_now() / 1000;
    if (cdata->last_washgold_refresh_time == 0) {
        cdata->last_washgold_refresh_time = now;
    }
    uint32_t diff = now - cdata->last_washgold_refresh_time;
    if (diff >= 3600) {
        cdata->washgold += min(1200, diff/60 * 2);
        cdata->last_washgold_refresh_time = now;
        if (sync) {
            sync_washgold_info(s, pr);
        }
    }
}

static void
process_washgold(struct module *s, struct player *pr) {
    struct hall *self = MODULE_SELF;
    struct chardata *cdata = &pr->data;
    if (cdata->washgold <= 0) {
        return;
    }
    uint32_t gain, extra;
    float rand = hall_luck_random_float(self, pr, 0.4, 100);
    if (rand * rand > 0.7) {
        gain = rand * 60;
    } else {
        gain = max(rand * 12, 4);
    }
    if (gain == 0) {
        return;
    }
    if (gain > cdata->washgold) {
        gain = cdata->washgold;
        cdata->washgold = 0;
    } else {
        cdata->washgold -= gain;
    }
    extra = gain * cdata->attri.coin_profit;
    cdata->coin += gain + extra;
    // no db hear
    sync_washgold_result(s, pr, gain, extra);
    //hall_sync_money(s, pr);
}

static void
login(struct module *s, struct player* pr) {
    refresh_washgold(s, pr, false);
}

void
hall_washgold_main(struct module *s, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_ENTERHALL:
        login(s, pr);
        break;
    case IDUM_WASHGOLD:
        process_washgold(s, pr);
        break;
    }
}

static void
timecb(void *pointer, void *ud) {
    struct module *s = ud;
    struct player *pr = pointer;
    refresh_washgold(s, pr, true);
}

void
hall_washgold_time(struct module *s) {
    struct hall *self = MODULE_SELF;
    if (SEC_ELAPSED(10)) {
        sh_hash_foreach2(&self->acc2player, timecb, s);
    }
}
