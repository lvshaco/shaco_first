#ifndef __robot_cmdctl_h__
#define __robot_cmdctl_h__

#include "robot.h"
#include "robot_tplt.h"
#include "cmdctl.h"

static int
reloadres(struct module *s, struct args *A, struct memrw *rw) {
    if (robot_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
nagent(struct agent_list *al) {
    int n = 0;
    struct agent *ag = al->head;
    while (ag) {
        n++;
        ag = ag->next;
    }
    return n;
}

static int
agent(struct module *s, struct args *A, struct memrw *rw) {
    struct robot *self = MODULE_SELF;
    int i, n, nc, nrest = 0;
    for (i=0; i<AI_MAX; ++i) {
        nc = nagent(&self->rests[i]);
        n = sh_snprintf(rw->ptr, RW_SPACE(rw), "[ai%d]%d ", i+1, nc);
        memrw_pos(rw, n);
        nrest += nc;
    }
    n = sh_snprintf(rw->ptr, RW_SPACE(rw), "[rest]%d ", nrest);
    memrw_pos(rw, n);
    n = sh_snprintf(rw->ptr, RW_SPACE(rw), "[all]%u", self->agents.used);
    memrw_pos(rw, n);
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
    } else if (!strcmp(cmd, "agent")) {
        return agent(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

#endif
