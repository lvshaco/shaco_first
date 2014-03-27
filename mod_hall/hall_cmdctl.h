#ifndef __hall_cmdctl_h__
#define __hall_cmdctl_h__

#include "cmdctl.h"
#include "hall.h"
#include "hall_tplt.h"

static int
reloadres(struct module* s, struct args* A, struct memrw* rw) {
    if (hall_tplt_main(s)) {
        return CTL_FAIL;
    }
    return CTL_OK;
}

static int
nuser(struct module *s, struct args *A, struct memrw *rw) {
    struct hall *self = MODULE_SELF;
    uint32_t np = self->acc2player.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(nplayer)", np);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static int
user(struct module *s, struct args *A, struct memrw *rw) {
    struct hall *self = MODULE_SELF;
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    int n;
    uint32_t accid = strtoul(A->argv[1], NULL, 10);
    struct player *pr = sh_hash_find(&self->acc2player, accid);
    if (pr) {
        n = snprintf(rw->ptr, RW_SPACE(rw), 
                    "accid(%u) charid(%u) name(%s) watchdog(%04x) "
                    "status(%d)",
                    pr->data.accid, pr->data.charid, pr->data.name, 
                    pr->watchdog_source,
                    pr->status);
    } else {
        n = snprintf(rw->ptr, RW_SPACE(rw), "none");
    }
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    //struct hall *self = MODULE_SELF;

    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "nuser")) {
        return nuser(s, &A, rw);
    } else if (!strcmp(cmd, "user")) {
        return user(s, &A, rw);
    } else if (!strcmp(cmd, "reloadres"))  {
        return reloadres(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
}

#endif
