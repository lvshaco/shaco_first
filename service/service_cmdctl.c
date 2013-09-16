#include "host_service.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_node.h"
#include "host_reload.h"
#include "node_type.h"
#include "user_message.h"
#include "args.h"
#include "memrw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

///////////////////
static int
_getloglevel(struct args* A, struct memrw* rw) {
    int n = snprintf(rw->begin, RW_SPACE(rw), "%s", host_log_levelstr(host_log_level()));
    memrw_pos(rw, n);
    return R_OK;
}

static int
_setloglevel(struct args* A, struct memrw* rw) {
    if (A->argc == 1)
        return R_ARGLESS;
    if (host_log_setlevelstr(A->argv[1]) == -1)
        return R_ARGINVALID;
    return R_OK;
}

static int
_reload(struct args* A, struct memrw* rw) {
    if (A->argc == 1)
        return R_ARGLESS;
    host_reload_prepare(A->argv[1]);
    return R_OK;
}

static int
_shownodecb(struct host_node* node, void* ud) {
    struct memrw* rw = ud;
    char tmp[HNODESTR_MAX];
    host_strnode(node, tmp);
    
    int n = snprintf(rw->ptr, RW_SPACE(rw), "%s\n", tmp);
    memrw_pos(rw, n);
    return R_OK;
}
static int
_shownode(struct args* A, struct memrw* rw) {
    int i;
    for (i=0; i<host_node_types(); ++i) {
        host_node_foreach(i, _shownodecb, rw);
    }
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
    SUBSCRIBE_MSG(s->serviceid, UMID_CMD_REQ);

    if (HNODE_TID(host_id()) == NODE_CENTER) {
        self->cmds_service = service_query_id("cmds");
        if (self->cmds_service == -1) {
            host_error("lost cmds service");
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
_cmdreq(struct cmdctl* self, int id, struct UM_base* um) {
    UM_CAST(UM_cmd_req, req, um);

    char* cmd = (char*)(req+1);
    size_t cl = req->msgsz - sizeof(*req);
    struct args A;
    args_parsestrl(&A, 2, cmd, cl);
    if (A.argc == 0) {
        return; // null
    }

    UM_DEFVAR(UM_cmd_res, res, UMID_CMD_RES);
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
        res->nodeid = host_id();
        service_notify_nodemsg(self->cmds_service, -1, res, res->msgsz);
    }
}

void
cmdctl_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct cmdctl* self = SERVICE_SELF;
    UM_CAST(UM_base, um, msg);
    switch (um->msgid) {
    case UMID_CMD_REQ:
        _cmdreq(self, id, msg);
        break;
    }
}
