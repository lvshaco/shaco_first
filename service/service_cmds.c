#include "sc_service.h"
#include "sh_util.h"
#include "sc.h"
#include "sc_log.h"
#include "sh_hash.h"
#include "user_message.h"
#include "cli_message.h"
#include "args.h"
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#define MODE_INTERACTIVE 0
#define MODE_CMDLINE 1

struct client {
    int gate_source;
    int connid;
    int mode;
    int nsend;
    int nrecv;
};

struct server {
    int ctl_handle;
    struct sh_hash clients;
};

struct server*
cmds_create() {
    struct server* self = malloc(sizeof(*self));
    return self;
}

void
cmds_free(struct server* self) {
    if (self == NULL)
        return;
    sh_hash_foreach(&self->clients, free);
    sh_hash_fini(&self->clients);
    free(self);
}

int
cmds_init(struct service* s) {
    struct server* self = SERVICE_SELF;
    if (sh_handler("cmdctl", &self->ctl_handle)) {
        return 1;
    }
    sh_hash_init(&self->clients, 1);
    return 0;
}

static inline void
notify_textinfo(struct service *s, int source, int connid, const char *text) {
    int len = strlen(text);
    UM_DEFWRAP2(UM_GATE, ga, len);
    ga->connid = connid;
    memcpy(ga->wrap, text, len);
    sh_service_send(SERVICE_ID, source, MT_UM, ga, sizeof(*ga)+len);
}

static inline void
check_close_client(struct service *s, struct client* cl) {
    struct server *self = SERVICE_SELF;
    // CMDLINE mode, deal one command, then close
    if (cl->mode == MODE_CMDLINE) {
        if (cl->nrecv >= cl->nsend) {
            UM_DEFWRAP(UM_GATE, ga, UM_LOGOUT, lo);
            ga->connid = cl->connid;
            lo->err = SERR_UNKNOW;
            sh_service_send(SERVICE_ID, cl->gate_source, MT_UM, ga, sizeof(*ga)+sizeof(*lo));

            cl = sh_hash_remove(&self->clients, cl->connid);
            free(cl);
        }
    }
}

static void
handle_result(struct service *s, int source, int connid, void *msg, int sz) {
    struct server *self = SERVICE_SELF;
    struct client *cl = sh_hash_find(&self->clients, connid);
    if (cl == NULL) {
        return;
    }
    char prefix[32];
    int npre = snprintf(prefix, sizeof(prefix), "[%08X] ", source);
    npre = min(npre, sizeof(prefix)-1);

    int len = sizeof(struct UM_TEXT) + sz + npre;
    UM_DEFWRAP2(UM_GATE, ga, len);
    UM_CAST(UM_TEXT, text, ga->wrap);
    memcpy(text->str, prefix, npre);
    memcpy(text->str+npre, msg, sz);
    sh_service_send(SERVICE_ID, cl->gate_source, MT_UM, ga, sizeof(*ga)+len);
     
    cl->nrecv++;
    check_close_client(s, cl);
}

static void
handle_command(struct service *s, int source, int connid, void *msg, int sz) {
    if (sz <= 0) {
        return;
    }
    struct server *self = SERVICE_SELF;
    struct client *cl = sh_hash_find(&self->clients, connid);
    if (cl == NULL) {
        cl = malloc(sizeof(*cl));
        cl->gate_source = source;
        cl->connid = connid;
        cl->mode = MODE_INTERACTIVE;
    }
    char* rptr = msg;
    cl->mode = *rptr++; sz--;
    cl->nsend = 0;
    cl->nrecv = 0;
    /*struct args A;
    args_parsestrl(&A, 3, rptr, sz);
    if (A.argc < 1) {
        notify_textinfo(s, source, connid, "usage: [node sid] command [arg1 arg2 .. ]");
        check_close_client(s, cl);
        return;
    }*/
    int wrapsz = sizeof(struct UM_TEXT)+sz;
    UM_DEFWRAP2(UM_CMDS, cm, wrapsz);
    UM_CAST(UM_TEXT, text, cm->wrap);
    cm->connid = connid;
    memcpy(text, rptr, sz);
    int n = sh_service_broadcast(SERVICE_ID, self->ctl_handle, MT_UM, cm, sizeof(*cm)+wrapsz);
    cl->nsend = n;
    check_close_client(s, cl);
}

static void
handle_disconnect(struct service *s, int source, int connid, int type) {
    struct server *self = SERVICE_SELF;
    if (type == NETE_TIMEOUT) {
        notify_textinfo(s, source, connid, "livetimeout.");
    }
    struct client *cl = sh_hash_remove(&self->clients, connid);
    if (cl) {
        free(cl);
    }
}

void
cmds_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_GATE: {
            UM_CASTCK(UM_GATE, ga, msg, sz);
            UM_CAST(UM_BASE, sub, ga->wrap);
            switch (sub->msgid) {
            case IDUM_NETDISCONN: {
                UM_CAST(UM_NETDISCONN, nd, sub);
                handle_disconnect(s, source, ga->connid, nd->type);
                break;
                }
            case IDUM_TEXT: {
                UM_CASTCK(UM_TEXT, text, sub, sz-sizeof(*ga));
                handle_command(s, source, ga->connid, text->str, sz-sizeof(*ga)-sizeof(*text));
                break;
                }
            }
            break;
            }
        case IDUM_CMDS: {
            UM_CAST(UM_CMDS, cm, msg);
            UM_CAST(UM_BASE, sub, cm->wrap);
            switch (sub->msgid) {
            case IDUM_TEXT: {
                UM_CASTCK(UM_TEXT, text, sub, sz-sizeof(*cm));
                handle_result(s, source, cm->connid, text->str, sz-sizeof(*cm)-sizeof(*text));
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
