#ifndef __node_type_h__
#define __node_type_h__

#include "message.h"
#include "host_node.h"
#include "host_log.h"

#define NODE_TYPE_MIN 0
#define NODE_CENTER 0
#define NODE_GATE   1
#define NODE_WORLD  2
#define NODE_GAME   3
#define NODE_TYPE_MAX 4

const char* NODE_NAMES[NODE_TYPE_MAX] = {
    "center", "gate", "world", "game",
};

struct node_message {
    const struct host_node* hn;
    struct UM_base* um;
};

static inline int
_decode_nodemessage(void* msg, int sz, struct node_message* nm) {
    const struct host_node* hn;
    UM_CAST(UM_base, um, msg);

    hn = host_node_get(um->nodeid);
    if (hn == NULL) {
        host_error("dismatch node %u, from msg %u", um->nodeid, um->msgid);
        return 1;
    }
    nm->hn = hn;
    nm->um = um;
    return 0;
}

#endif
