#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"
#include "msg_client.h"
#include "args.h"
#include "memrw.h"
#include <time.h>

struct cmdctl {
    bool is_center;
    int cmds_handle;
    int cmd_handle;
};

///////////////////

static inline bool
_iscenter(struct cmdctl *self) {
    return self->is_center;
}

///////////////////
static int
_getloglevel(struct module* s, struct args* A, struct memrw* rw) {
    int n = snprintf(rw->begin, RW_SPACE(rw), "%s", sh_log_levelstr(sh_log_level()));
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
_setloglevel(struct module* s, struct args* A, struct memrw* rw) {
    if (A->argc <= 1)
        return CTL_ARGLESS;
    if (sh_log_setlevelstr(A->argv[1]) == -1)
        return CTL_ARGINVALID;
    return CTL_OK;
}

static int
_reload(struct module* s, struct args* A, struct memrw* rw) {
    if (A->argc <= 1)
        return CTL_ARGLESS;
    int nload = sh_reload_prepare(A->argv[1]);
    if (nload > 0) {
        int n = snprintf(rw->ptr, RW_SPACE(rw), "reload %d", nload);
        memrw_pos(rw, n); 
        return CTL_OK;
    } else {
        return CTL_ARGINVALID;
    }
}

static int
_stop(struct module* s, struct args* A, struct memrw* rw) {
    if (!_iscenter(MODULE_SELF)) {
        sh_stop("stop command");
    }
    return CTL_OK;
}
static int
_start(struct module* s, struct args* A, struct memrw* rw) {
    if (_iscenter(MODULE_SELF)) {
        system("./shaco-foot startall");
    }
    return CTL_OK;
}
static int
_startmem(struct module* s, struct args* A, struct memrw* rw) {
    if (_iscenter(MODULE_SELF)) {
        system("./shaco-foot startall -m");
    }
    return CTL_OK;
}
static int
_time(struct module* s, struct args* A, struct memrw* rw) {
    uint64_t now = sh_timer_now();
    time_t sec = now / 1000;
    int n = strftime(rw->ptr, RW_SPACE(rw), "%y%m%d-%H:%M:%S", localtime(&sec));
    memrw_pos(rw, n);
    n = snprintf(rw->ptr, RW_SPACE(rw), "[%llu]", (unsigned long long int)sh_timer_elapsed());
    memrw_pos(rw, n);
    return CTL_OK;
}
/*
static int
_modcmd(struct module* s, struct args* A, struct memrw* rw) {
    struct cmdctl *self = MODULE_SELF;
    char cmd[1024];
    int i, n, off = 0;
    for (i=1; i<A->argc; ++i) {
        n = sh_snprintf(cmd+off, sizeof(cmd)-off, "%s ", A->argv[i]);
        if (n == 0) {
            return CTL_ARGTOOLONG;
        }
        off += n;
    }
    if (off == 0) {
        return CTL_ARGLESS;
    }
    sh_module_send(MODULE_ID, self->cmd_handle, MT_TEXT, cmd, off);
    return CTL_OK;
}
*/
///////////////////

static struct ctl_command CMDS[] = {
    { "getloglevel", _getloglevel },
    { "setloglevel", _setloglevel },
    { "reload",      _reload },
    { "stop",        _stop },
    { "start",       _start },
    { "startmem",    _startmem },
    { "time",        _time },
    //{ "modcmd",      _modcmd },
    { NULL, NULL },
};

///////////////////

struct cmdctl*
cmdctl_create() {
    struct cmdctl* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
cmdctl_free(struct cmdctl* self) {
    free(self);
}

int
cmdctl_init(struct module* s) {
    struct cmdctl* self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("cmds", SUB_REMOTE, &self->cmds_handle) ||
        sh_handler(sh_getstr("cmd_handle", ""), SUB_LOCAL, &self->cmd_handle)) {
        return 1;
    }
    int handle;
    if (sh_handler("centers", SUB_LOCAL, &handle))
        self->is_center = false;
    else
        self->is_center = true;
    return 0;
}

void
cmdctl_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct cmdctl *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_CMDS: {
            UM_CAST(UM_CMDS, cmd, msg);
            cmdctl_handle(s, source, cmd, sz, CMDS, self->cmd_handle);
            break;
            }
        }
        break;
        }
    }
}
