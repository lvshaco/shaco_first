#ifndef __hall_h__
#define __hall_h__

#include "sh.h"
#include "redis.h"
#include "hall_player.h"
#include "msg_server.h"
#include "msg_client.h"

#define TICK_INTV (1000)
#define SEC_TO_FLOAT_TICK(sec) ((1000.0/TICK_INTV)*sec)
#define SEC_TO_TICK(sec) (int)((SEC_TO_FLOAT_TICK(sec) < 1) ? 1 : (SEC_TO_FLOAT_TICK(sec)+0.5))
#define SEC_ELAPSED(sec) ((self->tick % SEC_TO_TICK(sec)) == 0)

struct tplt;

struct hall {
    struct tplt *T;
    int match_handle;
    int watchdog_handle;
    int rpuser_handle;
    int rprank_handle;
    int tick;
    uint32_t randseed;
    struct sh_hash acc2player;
    struct redis_reply reply;
};

static inline void
hall_notify_logout(struct module *s, struct player *pr, int err) {
    UM_DEFFIX(UM_EXITHALL, exit);
    exit->uid = UID(pr);
    exit->err = err;
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, exit, sizeof(*exit));
}

static inline void
hall_notify_login_fail(struct module *s, struct player *pr, int err) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_LOGINFAIL, lf);
    cl->uid = pr->data.accid;
    lf->err = err;
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl)+sizeof(*lf));
}

static inline void
hall_sync_role(struct module *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_CHARINFO, ci);
    cl->uid  = UID(pr);
    ci->data = pr->data;
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*ci));
}

static inline void
hall_sync_money(struct module *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_SYNCMONEY, sm);
    cl->uid  = UID(pr);
    sm->coin = pr->data.coin;
    sm->diamond = pr->data.diamond;
    sh_module_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*sm));
}

#endif
