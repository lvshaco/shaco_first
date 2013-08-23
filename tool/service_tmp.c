#include "host_service.h"
#include <stdlib.h>

struct _tmp {
};

struct _tmp*
tmp_create() {
    return NULL;
}

void
tmp_free(struct _tmp* self) {
}

int
tmp_init(struct service* s) {
    //struct _tmp* self = SERVICE_SELF;
    return 0;
}

void
tmp_service(struct service* s, struct service_message* sm) {
    //struct _tmp* self = SERVICE_SELF;
}

void
tmp_usermsg(struct service* s, int id, void* msg, int sz) {
    //struct _tmp* self = SERVICE_SELF;
}

void
tmp_net(struct service* s, struct net_message* nm) {
    /*struct _tmp* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        break;
    case NETE_ACCEPT:
        break;
    case NETE_CONNECT:
        break;
    case NETE_SOCKERR:
        break;
    case NETE_WRIDONE:
        break;
    }*/
}

void
tmp_time(struct service* s) {
    //struct _tmp* self= SERVICE_SELF;
}
