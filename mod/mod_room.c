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
    struct sh_monitor_handle h = { MODULE_ID, MODULE_ID };
    if (sh_monitor("robot", &h, &self->robot_handle) ||
        sh_monitor("watchdog", &h, &self->watchdog_handle) ||
        sh_monitor("match", &h, &self->match_handle))
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

static void 
umsg(struct module *s, int source, const void *msg, int sz) {
    struct room *self = MODULE_SELF;

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
}

struct exitud {
    struct module *s;
    int source;
};

static void
watchdog_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct player *m = pointer;
    if (m->watchdog_source == eu->source) {
        UM_DEFFIX(UM_LOGOUT, lo);
        lo->err = SERR_WATCHDOGEXIT;
        game_player_main(eu->s, m, lo, sizeof(*lo));
    }
}

static void
robot_exitcb(void *pointer, void *ud) {
    struct exitud *eu = ud;
    struct player *m = pointer;
    if (m->watchdog_source == eu->source) {
        UM_DEFFIX(UM_LOGOUT, lo);
        lo->err = SERR_ROBOTEXIT;
        game_player_main(eu->s, m, lo, sizeof(*lo));
    }
}

static void
monitor(struct module *s, int source, const void *msg, int sz) {
    struct room *self = MODULE_SELF;
    int type = sh_monitor_type(msg);
    int vhandle = sh_monitor_vhandle(msg);
    switch (type) {
    case MONITOR_START:
        break;
    case MONITOR_EXIT:
        if (vhandle == self->watchdog_handle) {
            struct exitud ud = { s, source };
            sh_hash_foreach2(&self->players, watchdog_exitcb, &ud);
        } else if (vhandle == self->match_handle) {
            // do nothing
        } else if (vhandle == self->robot_handle) {
            struct exitud ud = { s, source };
            sh_hash_foreach2(&self->players, robot_exitcb, &ud);
        }
        break;
    }
}

void
room_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM:
        umsg(s, source, msg, sz);
        break;
    case MT_MONITOR:
        monitor(s, source, msg, sz);
        break;
    case MT_CMD:
        cmdctl(s, source, msg, sz, command);
        break;
    }
}

void
room_time(struct module* s) {
    struct room* self = MODULE_SELF;
    game_time(s);
    self->tick++;
}
