#include "host_service.h"
#include "host.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_net.h"
#include "host_timer.h"
#include "host_assert.h"
#include "redis.h"
#include "user_message.h"
#include "node_type.h"
#include <stdlib.h>
#include <string.h>

struct worlddb {
    struct redis_reply reply;
};

struct worlddb*
worlddb_create() {
    struct worlddb* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
worlddb_free(struct worlddb* self) {
    redis_finireply(&self->reply);
    free(self);
}

int
worlddb_init(struct service* s) {
    struct worlddb* self = SERVICE_SELF;
    redis_initreply(&self->reply, 512, 0);
   
    SUBSCRIBE_MSG(s->serviceid, IDUM_REDISREPLY);
    host_timer_register(s->serviceid, 1000);
    return 0;
}

void
worlddb_service(struct service* s, struct service_message* sm) {
    //struct worlddb* self = SERVICE_SELF;
}

static void
_handleredisproxy(struct worlddb* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_REDISREPLY);

    UM_CAST(UM_REDISREPLY, rep, nm->um);
    int sz = rep->msgsz - sizeof(*rep);
    char tmp[64*1024];
    strncpy(tmp, rep->data, sz);
    tmp[sz] = '\0';
    //host_error(tmp);
    
    redis_resetreplybuf(&self->reply, rep->data, sz);
    hassertlog(redis_getreply(&self->reply) == REDIS_SUCCEED);
    //redis_walkreply(&self->reply);
}

void
worlddb_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct worlddb* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_REDISPROXY:
        _handleredisproxy(self, &nm);
        break;
    }
}

void
worlddb_time(struct service* s) {
    //struct worlddb* self= SERVICE_SELF;
}
