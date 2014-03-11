#ifndef __robot_cmdctl_h__
#define __robot_cmdctl_h__

#include "robot_tplt.h"
#include "cmdctl.h"

// command
static int
reloadres(struct module* s, struct args* A, struct memrw* rw) {
    if (robot_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static struct ctl_command CMDS[] = {
    { "reloadres", reloadres },
    { NULL, NULL },
};

#endif
