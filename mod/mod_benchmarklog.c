#include "sc.h"

struct benchmarklog {
    int log_handle;
    int times_persec;
    int tick;
};

struct benchmarklog *
benchmarklog_create() {
    struct benchmarklog *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
benchmarklog_free(struct benchmarklog *self) {
    free(self);
}

int
benchmarklog_init(struct module *s) {
    struct benchmarklog *self = MODULE_SELF;
    if (sh_handler("gamelog", SUB_REMOTE, &self->log_handle)) {
        return 1;
    }
    self->times_persec = sh_getint("benchmarklog_times_persec", 0);
    self->tick = 0;
    sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static void
send_log(struct module *s) {
    struct benchmarklog *self = MODULE_SELF;
    char msg[1024];
    int i;
    for (i=0; i<sizeof(msg); ++i) {
        msg[i] = i%26 + 'A';
    }
    sh_module_send(MODULE_ID, self->log_handle, MT_TEXT, msg, sizeof(msg)); 
}

void
benchmarklog_time(struct module *s) {
    struct benchmarklog *self = MODULE_SELF;
    int i;
    for (i=0; i<self->times_persec; ++i) {
        send_log(s);
    }
    self->tick++;
    sh_info("tick: %d, times: %d", self->tick, self->times_persec);
}
