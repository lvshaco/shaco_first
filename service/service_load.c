#include "host_service.h"
#include "host.h"
#include "host_node.h"
#include "host_dispatcher.h"
#include "node_type.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>

/*
 * 1. update the dest load
 * 2. receive msg from source, then route to the min load dest
 * 3. receive msg from dest,   then route to the source
 */
struct load {
    uint16_t source;
    uint16_t dest;
};

struct load*
load_create() {
    struct load* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
load_free(struct load* self) {
    free(self);
}

int
load_init(struct service* s) {
    struct load* self = SERVICE_SELF;
    self->source = host_node_typeid(host_getstr("load_source", ""));
    self->dest   = host_node_typeid(host_getstr("load_dest", ""));
    if (self->source == -1 ||
        self->dest == -1) {
        host_error("unknown load_source or load_dest");
        return 1;
    }
    // todo: some other good way todo this ?
    int i;
    for (i=IDUM_MINLOADBEGIN; i<IDUM_MINLOADEND; ++i) {
        SUBSCRIBE_MSG(s->serviceid, i);
    }
    SUBSCRIBE_MSG(s->serviceid, IDUM_FORWARD);
    SUBSCRIBE_MSG(s->serviceid, IDUM_UPDATELOAD);
    return 0;
}

void
load_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct load* self = SERVICE_SELF;
    UM_CAST(UM_BASE, um, msg);

    const struct host_node* node = host_node_get(um->nodeid);
    if (node == NULL) {
        return;
    }
    if (node->tid == self->source) {
        const struct host_node* dest = host_node_minload(self->dest);
        if (dest) {
            UM_DEFVAR(UM_FORWARD, fw);
            fw->cid = node->id;
            memcpy(&fw->wrap, um, um->msgsz);
            fw->wrap.nodeid = host_id();
            UM_SENDTONODE(dest, fw, UM_FORWARD_size(fw));
        } else {
            UM_DEFFIX(UM_MINLOADFAIL, fail);
            UM_SENDTONODE(node, fail, fail->msgsz);
        }
    } else if (node->tid == self->dest) {
        switch (um->msgid) {
        case IDUM_UPDATELOAD: {
            UM_CAST(UM_UPDATELOAD, load, um);
            host_node_setload(node->id, load->value);
            }
            break;
        case IDUM_FORWARD: {
            UM_CAST(UM_FORWARD, fw, um);
            const struct host_node* source = host_node_get(fw->cid);
            if (source) {
                UM_SENDTONODE(source, &fw->wrap, fw->wrap.msgsz);
            }
            }
            break;
        }
    }
}
