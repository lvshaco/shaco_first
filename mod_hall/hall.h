#ifndef __hall_h__
#define __hall_h__

#include "sh_hash.h"
#include "redis.h"
#include "hall_player.h"
#include "sc_node.h"
#include "msg_server.h"
#include "msg_client.h"

struct tplt;

struct hall {
    struct tplt *T;
    int match_handle;
    int watchdog_handle;
    int rpuser_handle;
    int rank_handle;
    struct sh_hash acc2player;
    struct redis_reply reply;
};

static inline void
hall_notify_logout(struct service *s, struct player *pr, int err) {
    UM_DEFFIX(UM_EXITHALL, exit);
    exit->uid = UID(pr);
    exit->err = err;
    sh_service_send(SERVICE_ID, pr->watchdog_source, MT_UM, exit, sizeof(*exit));
}

static inline void
hall_notify_login_fail(struct service *s, struct player *pr, int err) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_LOGINFAIL, lf);
    cl->uid = pr->data.accid;
    lf->err = err;
    sh_service_send(SERVICE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl)+sizeof(*lf));
}

static inline void
hall_sync_role(struct service *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_CHARINFO, ci);
    cl->uid  = UID(pr);
    ci->data = pr->data;
    sh_service_send(SERVICE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*ci));
}

#endif
