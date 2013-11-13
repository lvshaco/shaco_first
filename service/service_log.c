#include "host_service.h"
#include "host_timer.h"
#include "host_log.h"
#include "host.h"
#include "elog_include.h"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

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

    char logfile[PATH_MAX];
    const char* node = host_getstr("node_type", "");
    if (node[0] == '\0') {
        fprintf(stderr, "no config node_type\n");
        return 1;
    }
    printf("node: %s\n", node);
    snprintf(logfile, sizeof(logfile), "%s/log/%s%d.log", 
            getenv("HOME"), 
            node,
            host_getint("node_sid", 0));
    
    struct elog* el = elog_create(logfile);
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
    self->el = el;
    
    char msg[128];
    snprintf(msg, sizeof(msg), ">>> shaco host log level %s\n", 
            host_getstr("host_loglevel", ""));
    elog_append(self->el, msg, strlen(msg));
    return 0;
}

void
log_service(struct service* s, struct service_message* sm) {
    struct log* self = SERVICE_SELF;
    elog_append(self->el, sm->msg, sm->sz);
}
