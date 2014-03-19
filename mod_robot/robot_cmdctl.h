#ifndef __robot_cmdctl_h__
#define __robot_cmdctl_h__

#include "robot_tplt.h"
#include "cmdctl.h"

static int
reloadres(struct module* s, struct args* A, struct memrw* rw) {
    if (robot_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "reloadres")) {
        return reloadres(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

#endif
