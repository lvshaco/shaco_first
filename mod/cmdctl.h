#ifndef __cmdctl_h__
#define __cmdctl_h__

#include "sh.h"
#include "args.h"
#include "memrw.h"
#include "msg_server.h"
#include "msg_client.h"

#define LITERAL(str) str, sizeof(str)

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

static inline int 
_execute(struct module *s, struct args* A, struct memrw* rw,
        const struct ctl_command *cmdmap) {
    if (cmdmap) {
        const char* name = A->argv[0]; 
        const struct ctl_command* c = cmdmap;
        while (c->name) {
            if (strcmp(c->name, name) == 0 && c->fun) {
                return c->fun(s, A, rw);
            }
            c++;
        }
    }
    return CTL_NOCOMMAND;
}

static inline void
cmdctl_handle(struct module *s, int source, const void *msg, int sz, 
        const struct ctl_command *cmdmap, int forward_handle) {
    
    UM_CAST(UM_CMDS, cmd, msg); 
    UM_CAST(UM_BASE, sub, cmd->wrap);
    if (sub->msgid != IDUM_TEXT) {
        return;
    }
    UM_CASTCK(UM_TEXT, it, sub, sz - sizeof(*cmd));
    
    struct args A;
    args_parsestrl(&A, 0, it->str, sz - sizeof(*cmd) - sizeof(*it));
    if (A.argc == 0) {
        return; // null
    }

    UM_DEFVAR2(UM_CMDS, res, UM_MAXSZ);                                             
    UD_CAST(UM_TEXT, rt, res->wrap);                                              
    res->connid = cmd->connid;                                                           
    int headsz = sizeof(*res) + sizeof(*rt);                                      
    struct memrw rw;                                                                
    memrw_init(&rw, rt->str, UM_MAXSZ-headsz);                                    
                                                                                    
    int err = _execute(s, &A, &rw, cmdmap);
    if (err == CTL_NOCOMMAND) {
        if (forward_handle != -1) {
            sh_module_send(source, forward_handle, MT_CMD, cmd, sz);
            return;
        }
    }
    if (RW_EMPTY(&rw)) {
        int n = snprintf(rw.begin, rw.sz, "[%s] %s", A.argv[0], _strerror(err));
        memrw_pos(&rw, n);
    }
    sh_module_send(MODULE_ID, source, MT_UM, res, headsz + RW_CUR(&rw));
}

#endif
