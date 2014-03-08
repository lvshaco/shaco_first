#include "sh.h"
#include "room.h"
#include "msg_server.h"

#define PULL_CHECK if ((ro)->type == ROOM_TYPE_NORMAL) return;

static void
_pull(struct module *s, struct room_game *ro) {
    struct room *self = MODULE_SELF;
    int rand = sh_rand(self->randseed) % 100;
    if (rand > 70) {
        UM_DEFFIX(UM_ROBOT_PULL_REQ, pull); 
        pull->roomid = ro->id;
        sh_module_send(MODULE_ID, self->match_handle, MT_UM, pull, sizeof(*pull));
        ro->pull_next_time = 0;
        sh_trace("Room %u rand %d pull robot", ro->id, rand);
    } else {
        int off = 15 * (1+(70-rand)/70.f);
        ro->pull_next_time += off;
        sh_trace("Room %u rand %d pull robot %d S later", ro->id, rand, off);
    }
}

void
room_pull_init(struct module *s, struct room_game *ro) {
    PULL_CHECK;
    struct room *self = MODULE_SELF;
    if (room_preonline_1player(ro)) {
        int off = sh_rand(self->randseed) % (125 - 105) + 105;
        off = 30 * off/100.f;
        ro->pull_next_time = sh_timer_now()/1000 + off;
        sh_trace("Room %u init pull robot %d S later", ro->id, off);
    } else {
        ro->pull_next_time = 0;
    }
}

void
room_pull_update(struct module *s, struct room_game *ro) {
    PULL_CHECK;
    if (ro->pull_next_time > 0 &&
        ro->pull_next_time <= sh_timer_now()/1000) {
        _pull(s, ro);
    }
}

void
room_pull_on_join(struct module *s, struct room_game *ro) {
    PULL_CHECK;
    if (ro->pull_next_time > 0) {
        if (!room_preonline_1player(ro)) {
            ro->pull_next_time = 0;
        }
    }
}

void
room_pull_on_leave(struct module *s, struct room_game *ro) {
    PULL_CHECK;
    struct room *self = MODULE_SELF;
    if (room_preonline_1player(ro)) {
        int off = sh_rand(self->randseed)%(75-50)+50;
        off = 30 * off/100.f;
        ro->pull_next_time = sh_timer_now()/1000 + off;
        sh_trace("Room %u reinit pull robot %d S later", ro->id, off);
    }
}
