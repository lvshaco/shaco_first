#include "host_service.h"
#include <stdlib.h>

struct world {
};

struct world*
world_create() {
    return NULL;
}

void
world_free(struct world* self) {
}

int
world_init(struct service* s) {
    //struct world* self = SERVICE_SELF;
    return 0;
}

void
world_service(struct service* s, struct service_message* sm) {
    //struct world* self = SERVICE_SELF;
}

void
world_nodemsg(struct service* s, int id, void* msg, int sz) {
    //struct world* self = SERVICE_SELF;
}

void
world_net(struct service* s, struct net_message* nm) {
    /*struct world* self = SERVICE_SELF;
    switch (nm->type) {
    case NETE_READ:
        break;
    case NETE_ACCEPT:
        break;
    case NETE_CONNECT:
        break;
    case NETE_CONNERR:
        break;
    case NETE_SOCKERR:
        break;
    }*/
}

void
world_time(struct service* s) {
    //struct world* self= SERVICE_SELF;
}
