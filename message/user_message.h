#ifndef __user_message_h__
#define __user_message_h__

#include "host_net.h"
#include "host_node.h"
#include <stdint.h>
#include <stdlib.h>

#define UM_MAXSIZE 65536
#define UMID_MAX 65536
#define UMID_INVALID -1

#define UMID_NODE_REG 1
#define UMID_NODE_REGOK 2
#define UMID_NODE_SUB 3
#define UMID_NODE_NOTIFY 4

#pragma pack(1)

// nodeid: source node id where UM from
#define UM_header \
    uint16_t nodeid; \
    uint16_t msgsz; \
    uint16_t msgid;

struct user_message {
    UM_header;
    uint8_t data[0];
};

#define UM_HSIZE sizeof(user_message)

#define UM_DEF(um, n) \
    char um##data[n]; \
    struct user_message* um = (void*)um##data;

#define UM_DEFFIX(type, name, id) \
    struct type name; \
    name.msgid = id;

#define UM_DEFVAR(type, name, id) \
    char name##data[UM_MAXSIZE]; \
    struct type* name = (void*)name##data; \
    name->msgid = id;

#define UM_CAST(type, name, um) \
    struct type* name = (struct type*)um;

struct UM_node_reg {
    UM_header;
    uint32_t addr;
    uint16_t port;
};

struct UM_node_regok {
    UM_header;
    uint32_t addr;
    uint16_t port;
};

struct UM_node_subs {
    UM_header;
    uint16_t n;
    uint16_t subs[0];
};

static inline uint16_t 
UM_node_subs_size(struct UM_node_subs* um) {
    return sizeof(*um) + sizeof(um->subs[0]) * um->n;
}

struct UM_node_notify {
    UM_header;
    uint16_t tnodeid;
    uint32_t addr;
    uint16_t port;
};

#pragma pack()

static inline struct user_message*
UM_READ(int id, const char** error) {
    void* data;
    int size;
    struct user_message* h;
    h = host_net_read(id, sizeof(*h));
    if (h == NULL) {
        *error = host_net_error();
        return NULL;
    }
    size = h->msgsz - sizeof(*h);
    data = host_net_read(id, size);
    if (data == NULL) {
        *error = host_net_error();
        return NULL;
    }
    return h;
}

#define UM_SEND(id, um, sz) \
    (um)->nodeid = host_id(); \
    (um)->msgsz = sz; \
    host_net_send(id, um, sz);

#endif
