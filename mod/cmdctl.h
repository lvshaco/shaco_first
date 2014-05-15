#ifndef __cmdctl_h__
#define __cmdctl_h__

#include "sh.h"
#include "args.h"
#include "memrw.h"
#include "msg_server.h"
#include "msg_client.h"

struct ctl_command {
    const char* name;
    int (*fun)(struct module *s, struct args* A, struct memrw* rw);
};

#define CTL_OK 0
#define CTL_NOCMD 1
#define CTL_FAIL 2
#define CTL_ARGLESS 3
#define CTL_ARGINVALID 4
#define CTL_ARGTOOLONG 5
#define CTL_NOMODULE 6
#define CTL_SOMEFAIL 7
#define CTL_FORWARD 8

static const char* STRERROR[] = {
    "execute ok",
    "execute no command",
    "execute fail",
    "execute less arg",
    "execute invalid arg",
    "execute too long arg",
    "execute no module",
    "execute something fail",
    "", // forward no response
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
    return CTL_NOCMD;
}

static inline void
_response(struct module *s, int source, int connid, const char *msg, int len) {
    UM_DEFVAR2(UM_CMDS, res, UM_MAXSZ);                                             
    UD_CAST(UM_TEXT, text, res->wrap);                                              
    res->connid = connid;                                                           
    
    int headsz = sizeof(*res) + sizeof(*text);
    int bodysz = min(len, UM_MAXSZ - headsz);
    memcpy(text->str, msg, bodysz);

    sh_handle_send(MODULE_ID, source, MT_UM, res, headsz + bodysz);
}

typedef int (*cmdctl_handle_t)(
        struct module *s, 
        int source, 
        int connid, 
        const char *msg, 
        int len, 
        struct memrw *rw);

static inline void
cmdctl(struct module *s, int source, const void *msg, int sz, cmdctl_handle_t func) {
    UM_CAST(UM_CMDS, cmds, msg); 
    UM_CAST(UM_BASE, sub, cmds->wrap);
    if (sub->msgid != IDUM_TEXT) {
        return;
    }
    UM_CASTCK(UM_TEXT, text, sub, sz - sizeof(*cmds));

    char *cmd = text->str;
    int   len = sz - sizeof(*cmds) - sizeof(*text);

    char tmp[1024];
    struct memrw rw;                                                                
    memrw_init(&rw, tmp, sizeof(tmp));

    int err;
    if (func) {
        err = func(s, source, cmds->connid, cmd, len, &rw);
        if (err == CTL_FORWARD) {
            return; // forward mod deal
        }
    } else {
        err = CTL_NOCMD;
    }
    if (RW_EMPTY(&rw)) {
        int n = snprintf(rw.begin, rw.sz, "%s", _strerror(err));
        memrw_pos(&rw, n);
    }
    _response(s, source, cmds->connid, rw.begin, RW_CUR(&rw));
}

#endif
