#ifndef __match_cmdctl_h__
#define __match_cmdctl_h__

#include "cmdctl.h"
#include "match.h"

static int
nuser(struct module *s, struct args *A, struct memrw *rw) {
    struct match *self = MODULE_SELF;
    uint32_t napplyer = self->applyers.used;
    uint32_t nroom = self->rooms.used;
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%u(napplyer) %u(nroom)", napplyer, nroom);
    memrw_pos(rw, n); 
    return CTL_OK;
}

static int
user(struct module *s, struct args *A, struct memrw *rw) {
    struct match *self = MODULE_SELF;
    if (A->argc < 2) {
        return CTL_ARGLESS;
    }
    int n;
    uint32_t accid = strtoul(A->argv[1], NULL, 10);
    struct applyer *ar = sh_hash_find(&self->applyers, accid);
    if (ar) {
        n = snprintf(rw->ptr, RW_SPACE(rw), 
                    "uid(%u) accid(%u) charid(%u) name(%s) "
                    "robot(%d) type(%d) status(%d) roomid(%u) hall(%04x)",
                    ar->uid, ar->brief.accid, ar->brief.charid, ar->brief.name, 
                    ar->is_robot, ar->type, ar->status, ar->roomid, ar->hall_source);
    } else {
        n = snprintf(rw->ptr, RW_SPACE(rw), "none");
    }
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
    if (!strcmp(cmd, "nuser")) {
        return nuser(s, &A, rw);
    } else if (!strcmp(cmd, "user")) {
        return user(s, &A, rw);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

#endif
