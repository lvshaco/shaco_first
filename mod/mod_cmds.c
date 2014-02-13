#include "sh.h"
#include "msg_server.h"
#include "msg_client.h"
#include "args.h"

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
cmds_init(struct module* s) {
    struct server* self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("cmdctl", SUB_REMOTE, &self->ctl_handle)) {
        return 1;
    }
    sh_hash_init(&self->clients, 1);
    return 0;
}

static inline void
notify_textinfo(struct module *s, int source, int connid, const char *info) {
    int len = strlen(info);
    UM_DEFWRAP2(UM_GATE, ga, sizeof(struct UM_TEXT)+len);
    UD_CAST(UM_TEXT, text, ga->wrap);
    ga->connid = connid;
    memcpy(text->str, info, len);
    sh_module_send(MODULE_ID, source, MT_UM, ga, sizeof(*ga)+sizeof(*text)+len);
}

static inline void
check_close_client(struct module *s, struct client* cl) {
    struct server *self = MODULE_SELF;
    // CMDLINE mode, deal one command, then close
    if (cl->mode == MODE_CMDLINE) {
        if (cl->nsend > 0 &&
            cl->nrecv >= cl->nsend) {
            UM_DEFWRAP(UM_GATE, ga, UM_LOGOUT, lo);
            ga->connid = cl->connid;
            lo->err = SERR_OK;
            sh_module_send(MODULE_ID, cl->gate_source, MT_UM, ga, sizeof(*ga)+sizeof(*lo));

            cl = sh_hash_remove(&self->clients, cl->connid);
            free(cl);
        }
    }
}

static void
handle_result(struct module *s, int source, int connid, void *msg, int sz) {
    struct server *self = MODULE_SELF;
    struct client *cl = sh_hash_find(&self->clients, connid);
    if (cl == NULL) {
        return;
    }
    char prefix[32];
    int npre = snprintf(prefix, sizeof(prefix), "[%08X] ", source);
    npre = min(npre, sizeof(prefix)-1);
    int len = sizeof(struct UM_TEXT) + sz + npre;
    UM_DEFWRAP2(UM_GATE, ga, len);
    UD_CAST(UM_TEXT, text, ga->wrap);
    ga->connid = connid;
    memcpy(text->str, prefix, npre);
    memcpy(text->str+npre, msg, sz);
    sh_module_send(MODULE_ID, cl->gate_source, MT_UM, ga, sizeof(*ga)+len);
     
    cl->nrecv++;
    check_close_client(s, cl);
}

static void
handle_command(struct module *s, int source, int connid, void *msg, int sz) {
    if (sz <= 0) {
        return;
    }
    struct server *self = MODULE_SELF;
    struct client *cl = sh_hash_find(&self->clients, connid);
    if (cl == NULL) {
        cl = malloc(sizeof(*cl));
        cl->connid = connid;
        cl->gate_source = source;
        cl->mode = MODE_INTERACTIVE;
        assert(!sh_hash_insert(&self->clients, connid, cl));
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
    char tmp[1024];
    int l = min(sizeof(tmp)-1, sz);
    memcpy(tmp, rptr, l);
    tmp[l] = '\0';
    sh_rec("Cmd from %d: %s", connid, tmp);

    int wrapsz = sizeof(struct UM_TEXT)+sz;
    UM_DEFWRAP2(UM_CMDS, cm, wrapsz);
    UD_CAST(UM_TEXT, text, cm->wrap);
    cm->connid = connid;
    memcpy(text->str, rptr, sz);
    int n = sh_module_broadcast(MODULE_ID, self->ctl_handle, MT_UM, cm, sizeof(*cm)+wrapsz);
    cl->nsend = n;
    check_close_client(s, cl);
}

static void
handle_disconnect(struct module *s, int source, int connid, int type) {
    struct server *self = MODULE_SELF;
    if (type == NETE_TIMEOUT) {
        notify_textinfo(s, source, connid, "livetimeout.");
    }
    struct client *cl = sh_hash_remove(&self->clients, connid);
    if (cl) {
        free(cl);
    }
}

void
cmds_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
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
