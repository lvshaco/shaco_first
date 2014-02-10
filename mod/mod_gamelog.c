#include "sc.h"
#include "elog_include.h"

struct gamelog {
    struct elog *logger; 
    int count;
    uint64_t last_time;
};

struct gamelog *
gamelog_create() {
    struct gamelog *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gamelog_free(struct gamelog *self) {
    if (self == NULL)
        return;
    elog_free(self->logger);
    self->logger = NULL;
    free(self);
}

int
gamelog_init(struct module *s) {
    struct gamelog *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    struct elog *logger = elog_create("/tmp/gamelog.log");
    if (logger == NULL) {
        return 1;
    }
    if (elog_set_appender(logger, &g_elog_appender_file)) {
        sh_error("gamelog set appender fail");
        return 1;
    }
    self->logger = logger;
    return 0;
}

void
gamelog_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct gamelog *self = MODULE_SELF;
    assert(type == MT_TEXT);
   
    char tmp[1025];
    int size = min(sz, sizeof(tmp)-1);
    memcpy(tmp, msg, size);
    tmp[size] = '\0';
    elog_append(self->logger, tmp, size);

    self->count++;
    if (self->last_time == 0) {
        self->last_time = sh_timer_now();
    }
    uint64_t now = sh_timer_now();
    uint64_t diff = now - self->last_time;
    if (diff >= 1000) {
        int rate = self->count / (diff/1000.0);
        self->last_time = now;
        self->count = 0;
        sh_info("rate : %d", rate);
    }
}
