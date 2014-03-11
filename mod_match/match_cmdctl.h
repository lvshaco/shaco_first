#ifndef __match_cmdctl_h__
#define __match_cmdctl_h__

#include "cmdctl.h"
#include "match.h"

static int
playercount(struct module *s, struct args *A, struct memrw *rw) {
    struct match *self = MODULE_SELF;
    uint32_t napplyer = self->applyers.used;
    uint32_t nroom = self->rooms.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(napplyer) %u(nroom)", napplyer, nroom);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static struct ctl_command CMDS[] = {
    { "playercount", playercount },
    { NULL, NULL },
};

#endif
