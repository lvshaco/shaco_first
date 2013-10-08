#include "host_service.h"
#include <stdlib.h>

struct tmp {
};

struct tmp*
tmp_create() {
    return NULL;
}
void
tmp_free(struct tmp* self) {
}
int
tmp_init(struct service* s) {
    //struct tmp* self = SERVICE_SELF;
    return 0;
}
void
tmp_service(struct service* s, struct service_message* sm) {
    //struct tmp* self = SERVICE_SELF;
}
void
tmp_usermsg(struct service* s, int id, void* msg, int sz) {
}
void
tmp_nodemsg(struct service* s, int id, void* msg, int sz) {
    //struct tmp* self = SERVICE_SELF;
}
void
tmp_net(struct service* s, struct net_message* nm) {
    /*struct tmp* self = SERVICE_SELF;
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
tmp_time(struct service* s) {
    //struct tmp* self= SERVICE_SELF;
}
