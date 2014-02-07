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
benchmarklog_init(struct service *s) {
    struct benchmarklog *self = SERVICE_SELF;
    if (sh_handler("gamelog", SUB_REMOTE, &self->log_handle)) {
        return 1;
    }
    self->times_persec = sc_getint("benchmarklog_times_persec", 0);
    self->tick = 0;
    sc_timer_register(SERVICE_ID, 1000);
    return 0;
}

static void
send_log(struct service *s) {
    struct benchmarklog *self = SERVICE_SELF;
    char msg[1024];
    int i;
    for (i=0; i<sizeof(msg); ++i) {
        msg[i] = i%26 + 'A';
    }
    sh_service_send(SERVICE_ID, self->log_handle, MT_TEXT, msg, sizeof(msg)); 
}

void
benchmarklog_time(struct service *s) {
    struct benchmarklog *self = SERVICE_SELF;
    int i;
    for (i=0; i<self->times_persec; ++i) {
        send_log(s);
    }
    self->tick++;
    sc_info("tick: %d, times: %d", self->tick, self->times_persec);
}
