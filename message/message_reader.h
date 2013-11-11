#ifndef __message_reader_h__
#define __message_reader_h__

#include "message.h"
#include "host_net.h"
#include "host_service.h"

static inline struct UM_BASE*
_message_read_one(struct net_message* nm, int skip) {
    int id = nm->connid; 
    struct UM_BASE* base;
    void* data;
    int e;
    base = host_net_read(id, sizeof(*base), skip, &e);
    if (base == NULL) {
        goto null;
    }
    int sz = base->msgsz - sizeof(*base);
    if (sz != 0) {
        data = host_net_read(id, sz, 0, &e);
        if (data == NULL) {
            goto null;
        }
    }
    return base;
null:
    if (e) {
        // error occur, route to net service
        nm->type = NETE_SOCKERR;
        nm->error = e;
        service_notify_net(nm->ud, nm);
    }
    return NULL;
}

#endif
