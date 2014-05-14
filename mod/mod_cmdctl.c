#include "sh.h"
#include "cmdctl.h"
#include "msg_server.h"
#include "msg_client.h"
#include "args.h"
#include "memrw.h"
#include <time.h>

struct cmdctl {
    bool is_center;
};

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
    int handle;
    if (sh_handler("cmds", SUB_REMOTE, &handle)) {
        return 1;
    }
    if (module_query_id("centers") == -1)
        self->is_center = false;
    else
        self->is_center = true;
    return 0;
}

static int
local(struct module *s, const char *msg, int len, struct memrw *rw) {
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "getloglevel")) {
        int n = sh_snprintf(rw->begin, RW_SPACE(rw), "%s", sh_log_levelstr(sh_log_level()));
        memrw_pos(rw, n);

    } else if (!strcmp(cmd, "setloglevel")) {
        if (A.argc < 2)
            return CTL_ARGLESS;
        if (sh_log_setlevelstr(A.argv[1]) == -1)
            return CTL_ARGINVALID;
        else
            return CTL_OK;

    } else if (!strcmp(cmd, "reload")) {
        if (A.argc < 2)
            return CTL_ARGLESS;
        int nload = sh_reload_prepare(A.argv[1]);
        if (nload > 0) {
            int n = sh_snprintf(rw->ptr, RW_SPACE(rw), "reload %d", nload);
            memrw_pos(rw, n); 
            return CTL_OK;
        } else {
            return CTL_ARGINVALID;
        }

    } else if (!strcmp(cmd, "time")) {
        time_t start = sh_timer_start_time()/1000;
        time_t now = sh_timer_now()/1000;
        uint32_t elapsed = now - start;
        int h = elapsed/3600; elapsed %= 3600;
        int m = elapsed/60;   elapsed %= 60;
        int s = elapsed;
        int n;
        char sstart[32], snow[32];
        strftime(sstart, sizeof(sstart), "%y%m%d-%H:%M:%S", localtime(&start));
        strftime(snow, sizeof(snow), "%y%m%d-%H:%M:%S", localtime(&now));
        n = sh_snprintf(rw->ptr, RW_SPACE(rw), "%s ~ %s", sstart, snow);
        memrw_pos(rw, n);
        n = sh_snprintf(rw->ptr, RW_SPACE(rw), " E[%dh%dm%ds]", h, m, s); 
        memrw_pos(rw, n);
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    assert(len > 0);
    if (*msg == ':') { // specify mod command
        char *p = strchr(msg, ' ');
        if (p == NULL) {
            return CTL_NOCMD;
        }
        int subsz = len - (p+1-msg);
        if (subsz <= 0) {
            return CTL_NOCMD;
        }
        char name[64];
        int sz = min(p-(msg+1), sizeof(name)-1);
        memcpy(name, msg+1, sz);
        name[sz] = '\0';
        int handle = module_query_id(name);
        if (handle != -1) {
            UM_DEFVAR2(UM_CMDS, cmds, UM_MAXSZ);
            UD_CAST(UM_TEXT, text, cmds->wrap);
            cmds->connid = connid;
            int headsz = sizeof(*cmds) + sizeof(*text);
            int bodysz = min(UM_MAXSZ - headsz, subsz);
            memcpy(text->str, p+1, bodysz);
            sh_module_send(source, handle, MT_CMD, cmds, headsz + bodysz);
            return CTL_FORWARD;
        } else {
            return CTL_NOMODULE;
        }
    } else {
        return local(s, msg, len, rw);
    }
}

void
cmdctl_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_CMDS: {
            cmdctl(s, source, msg, sz, command);
            break;
            }
        }
        break;
        }
    }
}
