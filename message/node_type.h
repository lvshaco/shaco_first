#ifndef __node_type_h__
#define __node_type_h__

#include "message.h"
#include "sc_node.h"
#include "sc_log.h"

#define NODE_TYPE_MIN 0
#define NODE_CENTER 0
#define NODE_LOGIN  1
#define NODE_GATELOAD   2 
#define NODE_GATE   3
#define NODE_WORLD  4
#define NODE_GAME   5
#define NODE_BMDB   6
#define NODE_RPACC  7
#define NODE_RPUSER 8
#define NODE_RPRANK 9
#define NODE_TYPE_MAX 10

const char* NODE_NAMES[NODE_TYPE_MAX] = {
    "center", 
    "login", 
    "gateload", 
    "gate", 
    "world", 
    "game", 
    "bmdb", 
    "rpacc",
    "rpuser", 
    "rprank",
};

#define _NODEM_header \
    struct UM_BASE* um; \
    const struct sc_node* hn;

struct node_message {
    _NODEM_header;
};

static inline int
_decode_nodemessage(void* msg, int sz, struct node_message* nm) {
    const struct sc_node* hn;
    UM_CAST(UM_BASE, um, msg);

    hn = sc_node_get(um->nodeid);
    if (hn == NULL) {
        sc_error("dismatch node %u, from msg %u", um->nodeid, um->msgid);
        return 1;
    }
    nm->hn = hn;
    nm->um = um;
    return 0;
}

#endif
