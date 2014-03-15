#ifndef __room_cmdctl_h__
#define __room_cmdctl_h__

#include "cmdctl.h"
#include "room_game.h"
#include "room_tplt.h"

static int
reloadres(struct module *s, struct args *A, struct memrw *rw) {
    if (room_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
playercount(struct module *s, struct args *A, struct memrw *rw) {
    struct room *self = MODULE_SELF;
    uint32_t np = self->players.used;
    uint32_t nr = self->room_games.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(nplayer) %u(nroom)", np, nr);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static inline void
build_member(struct match_member *m, int id) {
    m->is_robot = false;
    m->brief.accid = id;
    m->brief.charid = id;
    m->brief.name[0] = '\0';
    m->brief.level = 1;
    m->brief.role = 10;
    m->brief.state = 10;
    m->brief.oxygen = 600;
    m->brief.body = 500;
    m->brief.quick = 500;
}

static int
test_gamecreate(struct module *s, struct args *A, struct memrw *rw) {
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    uint64_t t1 = sh_timer_elapsed_real();
    int times = strtol(A->argv[1], NULL, 10);
    int i;
    for (i=0; i<times; ++i) {
        int id = i+1;
        UM_DEFVAR(UM_CREATEROOM, create);
        create->type = 1;
        create->mapid = 1;
        create->id = id;
        create->max_member = 2;
        create->nmember = 2;
        build_member(&create->members[0], id*2+1);
        build_member(&create->members[1], id*2+2);
        game_main(s, -1, create, UM_CREATEROOM_size(create));
    }
    uint64_t t2 = sh_timer_elapsed_real();
    int n = snprintf(rw->ptr, RW_SPACE(rw), "times: %d, use time %u", times, (uint32_t)(t2-t1));
    memrw_pos(rw, n); 
    return CTL_OK;
}

static struct ctl_command CMDS[] = {
    { "reloadres", reloadres },
    { "playercount", playercount },
    { "test_gamecreate", test_gamecreate },
    { NULL, NULL },
};

#endif
