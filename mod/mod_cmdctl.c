#include "sh.h"
#include "msg_server.h"
#include "msg_client.h"
#include "args.h"
#include "memrw.h"
#include <time.h>

#define LITERAL(str) str, sizeof(str)

struct cmdctl {
    int cmds_handle;
    int cmd_handle;
};

///////////////////

struct ctl_command {
    const char* name;
    int (*fun)(struct module *s, struct args* A, struct memrw* rw);
};

#define CTL_OK 0
#define CTL_NOCOMMAND 1
#define CTL_FAIL 2
#define CTL_ARGLESS 3
#define CTL_ARGINVALID 4
#define CTL_ARGTOOLONG 5
#define CTL_NOMODULE 6

static const char* STRERROR[] = {
    "execute ok",
    "no command",
    "execute fail",
    "execute less arg",
    "execute invalid arg",
    "execute too long arg",
    "execute no module",
};

static inline const char*
_strerror(int error) {
    if (error >= 0 && error < sizeof(STRERROR)/sizeof(STRERROR[0]))
        return STRERROR[error];
    return "execute unknown error";
}

static inline bool
_ishenter() {
    return false; // todo
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
    if (!_ishenter()) {
        sh_stop("stop command");
    }
    return CTL_OK;
}
static int
_start(struct module* s, struct args* A, struct memrw* rw) {
    if (_ishenter()) {
        system("./shaco-foot startall");
    }
    return CTL_OK;
}
static int
_startmem(struct module* s, struct args* A, struct memrw* rw) {
    if (_ishenter()) {
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

static int
_reloadres(struct module* s, struct args* A, struct memrw* rw) {
    struct cmdctl *self = MODULE_SELF;
    if (sh_module_send(MODULE_ID, self->cmd_handle, MT_TEXT, LITERAL("reloadres"))) {
        return CTL_NOMODULE;
    }
    return CTL_OK;
}

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

///////////////////

static struct ctl_command COMMAND_MAP[] = {
    { "getloglevel", _getloglevel },
    { "setloglevel", _setloglevel },
    { "reload",      _reload },
    { "stop",        _stop },
    { "start",       _start },
    { "startmem",    _startmem },
    { "time",        _time },
    { "reloadres",   _reloadres },
    { "modcmd",      _modcmd },
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
    return 0;
}

static int 
execute(struct module *s, struct args* A, struct memrw* rw) {
    const char* name = A->argv[0]; 
    const struct ctl_command* c = COMMAND_MAP;
    while (c->name) {
        if (strcmp(c->name, name) == 0 && c->fun) {
            return c->fun(s, A, rw);
        }
        c++;
    }
    return CTL_NOCOMMAND;
}

static void
handle_command(struct module *s, int source, int connid, void *msg, int sz) {
    struct args A;
    args_parsestrl(&A, 0, msg, sz);
    if (A.argc == 0) {
        return; // null
    }
    UM_DEFVAR2(UM_CMDS, res, UM_MAXSZ);
    UD_CAST(UM_TEXT, text, res->wrap);
    res->connid = connid;
    int headsz = sizeof(*res) + sizeof(*text);
    int msgsz;
    struct memrw rw;
    memrw_init(&rw, text->str, UM_MAXSZ-headsz);

    int error = execute(s, &A, &rw);
    if (RW_EMPTY(&rw)) {
        int n = snprintf(rw.begin, rw.sz, "[%s] %s", A.argv[0], _strerror(error));
        msgsz = headsz + n;
    } else {
        msgsz = headsz + RW_CUR(&rw);
    }
    sh_module_send(MODULE_ID, source, MT_UM, res, msgsz);
}

void
cmdctl_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_CMDS: {
            UM_CAST(UM_CMDS, cm, msg);
            UM_CAST(UM_BASE, sub, cm->wrap);
            switch (sub->msgid) {
            case IDUM_TEXT: {
                UM_CASTCK(UM_TEXT, text, sub, sz-sizeof(*cm));
                handle_command(s, source, cm->connid, text->str, sz-sizeof(*cm)-sizeof(*text));
                break;
                }
            }
            break;
            }
        }
        break;
        }
    }
}
