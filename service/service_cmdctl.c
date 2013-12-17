#include "sc_service.h"
#include "sc_util.h"
#include "sc_env.h"
#include "cmdctl.h"
#include "sc.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sc_reload.h"
#include "sc_gate.h"
#include "node_type.h"
#include "user_message.h"
#include "args.h"
#include "memrw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct cmdctl {
    int cmds_service;
    int cmdctl_handler;
    int tplt_handler;
};

///////////////////

static const char* STRERROR[] = {
    "execute ok",
    "no command",
    "execute fail",
    "execute less arg",
    "execute invalid arg",
    "execute no service",
};

static inline const char*
_strerror(int error) {
    if (error >= 0 && error < sizeof(STRERROR)/sizeof(STRERROR[0]))
        return STRERROR[error];
    return "execute unknown error";
}

static inline bool
_iscenter() {
    return (HNODE_TID(sc_id()) == NODE_CENTER);
}

///////////////////
static int
_getloglevel(struct cmdctl* self, struct args* A, struct memrw* rw) {
    int n = snprintf(rw->begin, RW_SPACE(rw), "%s", sc_log_levelstr(sc_log_level()));
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
_setloglevel(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (A->argc <= 1)
        return CTL_ARGLESS;
    if (sc_log_setlevelstr(A->argv[1]) == -1)
        return CTL_ARGINVALID;
    return CTL_OK;
}

static int
_reload(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (A->argc <= 1)
        return CTL_ARGLESS;
    sc_reload_prepare(A->argv[1]);
    return CTL_OK;
}

static int
_shownodecb(const struct sc_node* node, void* ud) {
    struct memrw* rw = ud;
    char tmp[HNODESTR_MAX];
    sc_strnode(node, tmp);
    
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%s\n", tmp);
    memrw_pos(rw, n);
    return 0;
}
static int
_shownode(struct cmdctl* self, struct args* A, struct memrw* rw) {
    int i;
    for (i=0; i<sc_node_types(); ++i) {
        sc_node_foreach(i, _shownodecb, rw);
    }
    return CTL_OK;
}
static int
_stop(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (!_iscenter()) {
        sc_stop();
    }
    return CTL_OK;
}
static int
_start(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (_iscenter()) {
        system("./shaco-foot startall");
    }
    return CTL_OK;
}
static int
_startmem(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (_iscenter()) {
        system("./shaco-foot startall -m");
    }
    return CTL_OK;
}
static int
_time(struct cmdctl* self, struct args* A, struct memrw* rw) {
    uint64_t now = sc_timer_now();
    time_t sec = now / 1000;
    int n = strftime(rw->ptr, RW_SPACE(rw), "%y%m%d-%H:%M:%S", localtime(&sec));
    memrw_pos(rw, n);
    n = snprintf(rw->ptr, RW_SPACE(rw), "[%llu]", (unsigned long long int)sc_timer_elapsed());
    memrw_pos(rw, n);
    return CTL_OK;
}
static int
_players(struct cmdctl* self, struct args* A, struct memrw* rw) {
    int count = sc_gate_usedclient();
    int n = snprintf(rw->ptr, RW_SPACE(rw), "[%d]", count);
    memrw_pos(rw, n);
    return CTL_OK;
}

static int
_reloadres(struct cmdctl* self, struct args* A, struct memrw* rw) {
    if (self->tplt_handler == SERVICE_INVALID) {
        return CTL_NOSERVICE;
    }
    struct service_message sm = {0, 0, sc_cstr_to_int32("TPLT"), 0, NULL};
    service_notify_service(self->tplt_handler, &sm);
    return CTL_OK;
}

static int
_db(struct cmdctl* self, struct args* A, struct memrw* rw) {
    int handler = service_query_id("benchmarkdb");
    if (handler == SERVICE_INVALID) {
        return CTL_NOSERVICE;
    }
    if (A->argc <= 4) {
        return CTL_ARGLESS;
    }
    char* type = A->argv[1];
    int start = strtol(A->argv[2], NULL, 10);
    int count = strtol(A->argv[3], NULL, 10);
    int init  = strtol(A->argv[4], NULL, 10);
    struct service_message sm = {start, 0, count, init, type};
    service_notify_service(handler, &sm);
    return CTL_OK;
}

///////////////////

static struct ctl_command COMMAND_MAP[] = {
    { "getloglevel", _getloglevel },
    { "setloglevel", _setloglevel },
    { "reload",      _reload },
    { "shownode",    _shownode },
    { "stop",        _stop },
    { "start",       _start },
    { "startmem",    _startmem },
    { "time",        _time },
    { "players",     _players },
    { "reloadres",   _reloadres },
    { "db",          _db },
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
cmdctl_init(struct service* s) {
    struct cmdctl* self = SERVICE_SELF;
    SUBSCRIBE_MSG(s->serviceid, IDUM_CMDREQ);

    if (HNODE_TID(sc_id()) == NODE_CENTER) {
        if (sc_handler("cmds", &self->cmds_service))
            return 1;
    } else {
        self->cmds_service = SERVICE_INVALID;
    }
    self->cmdctl_handler = service_query_id(sc_getstr("cmdctl_handler", ""));
    self->tplt_handler   = service_query_id(sc_getstr("tplt_handler", ""));
    return 0;
}

static const struct ctl_command*
_lookupcmd(const struct ctl_command* cmdmap, const char* name) {
    const struct ctl_command* c = cmdmap;
    while (c->name) {
        if (strcmp(c->name, name) == 0 && c->fun)
            return c;
        c++;
    }
    return NULL;
}

static int 
_execute(struct cmdctl* self, struct args* A, struct memrw* rw) {
    const char* name = A->argv[0]; 

    const struct ctl_command* c = NULL;
    if (self->cmdctl_handler != SERVICE_INVALID) {
        struct service_message sm = {0, 0, 0, 0, NULL};
        service_notify_service(self->cmdctl_handler, &sm);
        if (sm.msg != NULL) {
            struct ctl_command* extramap = sm.msg;
            c = _lookupcmd(extramap, name);
        }
    }
    if (c == NULL) {
        c = _lookupcmd(COMMAND_MAP, name);
    }
    if (c) {
        return c->fun(self, A, rw);
    }
    return CTL_NOCOMMAND;
}

static void
_cmdreq(struct cmdctl* self, int id, struct UM_BASE* um) {
    UM_CAST(UM_CMDREQ, req, um);

    char* cmd = (char*)(req+1);
    size_t cl = req->msgsz - sizeof(*req);
    struct args A;
    args_parsestrl(&A, 0, cmd, cl);
    if (A.argc == 0) {
        return; // null
    }

    UM_DEFVAR(UM_CMDRES, res);
    res->cid = req->cid; 
    struct memrw rw;
    memrw_init(&rw, res+1, res->msgsz-sizeof(*res));

    int error = _execute(self, &A, &rw);
    if (RW_EMPTY(&rw)) {
        int n = snprintf(rw.begin, rw.sz, "[%s] %s", A.argv[0], _strerror(error));
        res->msgsz = sizeof(*res) + n;
    } else {
        res->msgsz = sizeof(*res) + RW_CUR(&rw);
    }
    
    if (self->cmds_service == SERVICE_INVALID) {
        UM_SEND(id, res, res->msgsz);
    } else {
        res->nodeid = sc_id();
        service_notify_nodemsg(self->cmds_service, -1, res, res->msgsz);
    }
}

void
cmdctl_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct cmdctl* self = SERVICE_SELF;
    UM_CAST(UM_BASE, um, msg);
    switch (um->msgid) {
    case IDUM_CMDREQ:
        _cmdreq(self, id, msg);
        break;
    }
}
