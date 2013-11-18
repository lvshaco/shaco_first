#ifndef __node_type_h__
#define __node_type_h__

#include "message.h"
#include "sc_node.h"
#include "sc_log.h"

#define NODE_TYPE_MIN 0
#define NODE_CENTER 0
#define NODE_GATE   1
#define NODE_GAME   2 
#define NODE_WORLD  3 
#define NODE_REDISPROXY 4
#define NODE_BENCHMARKDB 5
#define NODE_LOGIN 6
#define NODE_LOAD 7
#define NODE_TYPE_MAX 8

const char* NODE_NAMES[NODE_TYPE_MAX] = {
    "center", 
    "gate", 
    "game", 
    "world", 
    "redisproxy", 
    "benchmarkdb", 
    "login", 
    "load",
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
