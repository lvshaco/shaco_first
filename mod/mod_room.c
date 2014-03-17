#include "sh.h"
#include "room.h"
#include "room_game.h"
#include "room_cmdctl.h"
#include "msg_server.h"

struct room*
room_create() {
    struct room* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
room_free(struct room* self) {
    if (self == NULL)
        return;
    game_fini(self);
    room_tplt_fini(self);
    free(self);
}

int
room_init(struct module* s) {
    struct room* self = MODULE_SELF;

    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    int handle;
    if (sh_handler("robot", SUB_REMOTE, &handle) ||
        sh_handler("watchdog", SUB_REMOTE, &handle) ||
        sh_handler("match", SUB_REMOTE, &self->match_handle))
        return 1;
   
    if (room_tplt_init(self)) {
        return 1;
    }
    if (game_init(self)) {
        return 1;
    }
    sh_timer_register(MODULE_ID, TICK_INTV);
    return 0;
}

void
room_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct room *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ROOM: {
            UM_CAST(UM_ROOM, ha, msg);
            UM_CAST(UM_BASE, wrap, ha->wrap);
            struct player *m = sh_hash_find(&self->players, ha->uid);
            if (m) {
                game_player_main(s, m, wrap, sz - sizeof(*ha));
            }
            break;
            }
        default:
            game_main(s, source, msg, sz);
            break;
        }
        break;
        }
    case MT_MONITOR:
        // todo
        break;
    case MT_CMD:
        cmdctl_handle(s, source, msg, sz, CMDS, -1);
        break;
    }
}

void
room_time(struct module* s) {
    struct room* self = MODULE_SELF;
    game_time(s);
    self->tick++;
}
