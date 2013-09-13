#ifndef __user_message_h__
#define __user_message_h__

#include "host_net.h"
#include <stdint.h>
#include <stdlib.h>

#define UMID_MAX 65536
#define UMID_INVALID -1

#define UMID_NODE_REG 1

#pragma pack(1)
#define UM_header \
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

#define UM_DEFFIX(type, name, msgid) \
    strcut type name; \
    name.msgid = msgid;

#define UM_DEFVAR(type, name, msgid, n) \
    char name##data[n]; \
    struct type* name = (void*)name##data; \
    name->msgid = msgid;

struct UM_node_reg {
    UM_header;
    uint16_t tid;
    uint16_t sid;
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

#pragma pack()

struct user_message*
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
    (um)->msgsz = sz; \
    host_net_send(id, um, sz);

#endif
