#include "sc_service.h"
#include <stdlib.h>

struct tmp {
};

struct tmp *
tmp_create() {
    return NULL;
}

void
tmp_free(struct tmp *self) {
}

int
tmp_init(struct service *s) {
    //struct tmp *self = SERVICE_SELF;
    return 0;
}

void
tmp_time(struct service *s) {
    //struct tmp *self = SERVICE_SELF;
}

void
tmp_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    //struct tmp *self = SERVICE_SELF;
}
