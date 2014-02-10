#include "sc.h"
#include "elog_include.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

struct log {
    struct elog* el;
};

struct log*
log_create() {
    struct log* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
log_free(struct log* self) {
    if (self->el) {
        elog_free(self->el);
        self->el = NULL;
    }
    free(self);
}

int
log_init(struct service* s) {
    struct log* self = SERVICE_SELF;
    
    struct elog* el;
    if (sc_getint("sc_daemon", 0)) { 
        const char* logdir = sc_getstr("log_dir", "");
        if (logdir[0] == '\0') {
            fprintf(stderr, "no specify log dir\n");
            return 1;
        }
        mkdir(logdir, 0744);
        char logfile[PATH_MAX];
        snprintf(logfile, sizeof(logfile), "%s/%d.log",
                logdir,
                sc_getint("node_id", 0));

        el = elog_create(logfile);
        if (el == NULL) {
            return 1;
        }
        if (elog_set_appender(el, &g_elog_appender_rollfile)) {
            fprintf(stderr, "elog set appender fail\n");
            return 1;
        }
        struct elog_rollfile_conf conf;
        conf.file_max_num = 10;
        conf.file_max_size = 1024*1024*1024;
        elog_appender_rollfile_config(el, &conf);
    } else {
        el = elog_create("");
        if (el == NULL) {
            return 1;
        }
        if (elog_set_appender(el, &g_elog_appender_file)) {
            fprintf(stderr, "elog set appender fail\n");
            return 1;
        }
    }

    self->el = el;
    
    char msg[128];
    snprintf(msg, sizeof(msg), ">>> shaco(%d) sc log level %s\n", 
            sc_getint("node_id", 0), sc_getstr("sc_loglevel", ""));
    elog_append(self->el, msg, strlen(msg));
    return 0;
}

void
log_main(struct service* s, int session, int source, int type, const void *msg, int sz) {
    struct log* self = SERVICE_SELF;
    elog_append(self->el, msg, sz);
}
