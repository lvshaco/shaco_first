#ifndef __room_cmdctl_h__
#define __room_cmdctl_h__

#include "cmdctl.h"
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

static struct ctl_command CMDS[] = {
    { "reloadres", reloadres },
    { "playercount", playercount },
    { NULL, NULL },
};

#endif
