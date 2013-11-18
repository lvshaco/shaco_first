#include "sc_service.h"
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

///////////////////
#define R_OK 0
#define R_NOCOMMAND 1
#define R_FAIL 2
#define R_ARGLESS 3
#define R_ARGINVALID 4

static const char* STRERROR[] = {
    "execute ok",
    "no command",
    "execute fail",
    "execute less arg",
    "execute invalid arg",
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
_getloglevel(struct args* A, struct memrw* rw) {
    int n = snprintf(rw->begin, RW_SPACE(rw), "%s", sc_log_levelstr(sc_log_level()));
    memrw_pos(rw, n);
    return R_OK;
}

static int
_setloglevel(struct args* A, struct memrw* rw) {
    if (A->argc == 1)
        return R_ARGLESS;
    if (sc_log_setlevelstr(A->argv[1]) == -1)
        return R_ARGINVALID;
    return R_OK;
}

static int
_reload(struct args* A, struct memrw* rw) {
    if (A->argc == 1)
        return R_ARGLESS;
    sc_reload_prepare(A->argv[1]);
    return R_OK;
}

static int
_shownodecb(const struct sc_node* node, void* ud) {
    struct memrw* rw = ud;
    char tmp[HNODESTR_MAX];
    sc_strnode(node, tmp);
    
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%s\n", tmp);
    memrw_pos(rw, n);
    return R_OK;
}
static int
_shownode(struct args* A, struct memrw* rw) {
    int i;
    for (i=0; i<sc_node_types(); ++i) {
        sc_node_foreach(i, _shownodecb, rw);
    }
    return R_OK;
}
static int
_stop(struct args* A, struct memrw* rw) {
    if (!_iscenter()) {
        sc_stop();
    }
    return R_OK;
}
static int
_start(struct args* A, struct memrw* rw) {
    if (_iscenter()) {
        system("./start");
    }
    return R_OK;
}
static int
_startmem(struct args* A, struct memrw* rw) {
    if (_iscenter()) {
        system("./start-memcheck");
    }
    return R_OK;
}
static int
_time(struct args* A, struct memrw* rw) {
    uint64_t now = sc_timer_now();
    time_t sec = now / 1000;
    int n = strftime(rw->ptr, RW_SPACE(rw), "%y%m%d-%H:%M:%S", localtime(&sec));
    memrw_pos(rw, n);
    n = snprintf(rw->ptr, RW_SPACE(rw), "[%llu]", (unsigned long long int)sc_timer_elapsed());
    memrw_pos(rw, n);
    return R_OK;
}
static int
_players(struct args* A, struct memrw* rw) {
    int count = sc_gate_usedclient();
    int n = snprintf(rw->ptr, RW_SPACE(rw), "[%d]", count);
    memrw_pos(rw, n);
    return R_OK;
}

///////////////////
struct command {
    const char* name;
    int (*fun)(struct args* A, struct memrw* rw);
};

static struct command COMMAND_MAP[] = {
    { "getloglevel", _getloglevel },
    { "setloglevel", _setloglevel },
    { "reload",      _reload },
    { "shownode",    _shownode },
    { "stop",        _stop },
    { "start",       _start },
    { "startmem",    _startmem },
    { "time",        _time },
    { "players",     _players },
    { NULL, NULL },
};

///////////////////
struct cmdctl {
    int cmds_service;
};

struct cmdctl*
cmdctl_create() {
    struct cmdctl* self = malloc(sizeof(*self));
    self->cmds_service = -1;
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
        self->cmds_service = service_query_id("cmds");
        if (self->cmds_service == -1) {
            sc_error("lost cmds service");
            return 1;
        }
    } else {
        self->cmds_service = -1;
    }
    return 0;
}

static struct command*
_lookupcmd(const char* name) {
    struct command* c = &COMMAND_MAP[0];
    while (c->name) {
        if (strcmp(c->name, name) == 0 &&
            c->fun)
            return c;
        c++;
    }
    return NULL;
}

static int 
_execute(struct args* A, struct memrw* rw) {
    struct command* c = _lookupcmd(A->argv[0]);
    if (c)
        return c->fun(A, rw);
    return R_NOCOMMAND;
}

static void
_cmdreq(struct cmdctl* self, int id, struct UM_BASE* um) {
    UM_CAST(UM_CMDREQ, req, um);

    char* cmd = (char*)(req+1);
    size_t cl = req->msgsz - sizeof(*req);
    struct args A;
    args_parsestrl(&A, 2, cmd, cl);
    if (A.argc == 0) {
        return; // null
    }

    UM_DEFVAR(UM_CMDRES, res);
    res->cid = req->cid; 
    struct memrw rw;
    memrw_init(&rw, res+1, res->msgsz-sizeof(*res));

    int error = _execute(&A, &rw);
    if (RW_EMPTY(&rw)) {
        int n = snprintf(rw.begin, rw.sz, "[%s] %s", A.argv[0], _strerror(error));
        res->msgsz = sizeof(*res) + n;
    } else {
        res->msgsz = sizeof(*res) + RW_CUR(&rw);
    }
    
    if (self->cmds_service == -1) {
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
