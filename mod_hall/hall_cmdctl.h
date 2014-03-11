#ifndef __hall_cmdctl_h__
#define __hall_cmdctl_h__

#include "cmdctl.h"
#include "hall.h"
#include "hall_tplt.h"

// command
static int
reloadres(struct module* s, struct args* A, struct memrw* rw) {
    if (hall_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
playercount(struct module *s, struct args *A, struct memrw *rw) {
    struct hall *self = MODULE_SELF;
    uint32_t np = self->acc2player.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(nplayer)", np);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static struct ctl_command CMDS[] = {
    { "reloadres", reloadres },
    { "playercount", playercount },
    { NULL, NULL },
};

#endif
