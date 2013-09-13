#ifndef __node_type_h__
#define __node_type_h__

#define NODE_CENTER 0
#define NODE_GATE   1
#define NODE_WORLD  2
#define NODE_GAME   3
#define NODE_MAX    4

static const char* NODE_NAMES[NODE_MAX] = {
    "center", "gate", "world", "game",
};

#endif
