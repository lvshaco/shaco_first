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


static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    //struct match *self = MODULE_SELF;

    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "playercount")) {
        return playercount(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

#endif
