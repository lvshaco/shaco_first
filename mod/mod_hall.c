#include "sh.h"
#include "hall.h"
#include "hall_tplt.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "hall_role.h"
#include "hall_ring.h"
#include "hall_award.h"
#include "hall_play.h"
#include "hall_washgold.h"
#include "msg_server.h"
#include "msg_client.h"

struct hall *
hall_create() {
    struct hall *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
hall_free(struct hall *self) {
    if (self == NULL)
        return;
    hall_player_fini(self);
    hall_playerdb_fini(self); 
    hall_tplt_fini(self);
    free(self);
}

int
hall_init(struct module *s) {
    struct hall *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("watchdog", SUB_REMOTE, &self->watchdog_handle) ||
        sh_handler("match", SUB_REMOTE, &self->match_handle) ||
        sh_handler("rpuser", SUB_REMOTE, &self->rpuser_handle) ||
        sh_handler("rprank", SUB_REMOTE, &self->rprank_handle)) {
        return 1;
    }
    self->randseed = sh_timer_now()/1000;
    if (hall_tplt_init(self))
        return 1;
    if (hall_player_init(self))
        return 1;
    if (hall_playerdb_init(self))
        return 1; 

    sh_timer_register(MODULE_ID, TICK_INTV);
    return 0;
}

void
hall_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct hall *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ENTERHALL:
            hall_player_main(s, source, NULL, msg, sz);
            break;
        case IDUM_EXITROOM: {
            UM_CAST(UM_EXITROOM, exit, msg);
            struct player *pr = sh_hash_find(&self->acc2player, exit->uid);
            if (pr) {
                hall_play_main(s, pr, msg, sz);
            }
            break;
            }
        case IDUM_HALL: {
            UM_CAST(UM_HALL, ha, msg);
            struct player *pr = sh_hash_find(&self->acc2player, ha->uid);
            if (pr == NULL)
                return;
            UM_CAST(UM_BASE, wrap, ha->wrap);
            switch (wrap->msgid) {
            case IDUM_LOGOUT:
                hall_playerdb_send(s, pr, PDB_SAVE);
                hall_player_main(s, source, pr, wrap, sz-sizeof(*ha));
                break;
            case IDUM_CHARCREATE:
                hall_player_main(s, source, pr, wrap, sz-sizeof(*ha));
                break;
            default:
                if (wrap->msgid >= IDUM_AWARDB && wrap->msgid <= IDUM_AWARDE) {
                    hall_award_main(s, pr, wrap, sz-sizeof(*ha)); 
                } else if (wrap->msgid >= IDUM_ROLEB && wrap->msgid <= IDUM_ROLEE) {
                    hall_role_main(s, pr, wrap, sz-sizeof(*ha));
                } else if (wrap->msgid >= IDUM_RINGB && wrap->msgid <= IDUM_RINGE) {
                    hall_ring_main(s, pr, wrap, sz-sizeof(*ha));
                } else if (wrap->msgid >= IDUM_PLAYB && wrap->msgid <= IDUM_PLAYE) {
                    hall_play_main(s, pr, wrap, sz-sizeof(*ha));
                } else if (wrap->msgid >= IDUM_WASHGOLDB && wrap->msgid <= IDUM_WASHGOLDE) {
                    hall_washgold_main(s, pr, wrap, sz-sizeof(*ha));
                }
                break;
            }
            break;
            }
        case IDUM_MATCH: {
            UM_CAST(UM_MATCH, ma, msg);
            struct player *pr = sh_hash_find(&self->acc2player, ma->uid);
            if (pr == NULL)
                return;
            UM_CAST(UM_BASE, wrap, ma->wrap);
            hall_play_main(s, pr, wrap, sz-sizeof(*ma));
            break;
            }
        case IDUM_REDISREPLY: {
            UM_CAST(UM_REDISREPLY, rr, msg);
            hall_playerdb_process_redis(s, rr, sz);
            break;
            }
        }
        break;
        }
    case MT_TEXT:
        hall_tplt_main(s, session, source, type, msg, sz);
        break;
    case MT_MONITOR:
        break;
    }
}

void
hall_time(struct module *s) {
    struct hall *self = MODULE_SELF;
    hall_washgold_time(s);
    self->tick++;
}
