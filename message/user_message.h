#ifndef __user_message_h__
#define __user_message_h__

#include "host_net.h"
#include "host_node.h"
#include <stdint.h>
#include <stdlib.h>

#define UM_MAXSIZE 60000
#define UMID_MAX 65536
#define UMID_INVALID -1

#define UMID_NODE_REG 1
#define UMID_NODE_REGOK 2
#define UMID_NODE_SUB 3
#define UMID_NODE_NOTIFY 4
#define UMID_CMD_REQ 10
#define UMID_CMD_RES 11

#pragma pack(1)

// nodeid: source node id where UM from
#define _NODE_header \
    uint16_t nodeid;

#define _UM_header \
    _NODE_header; \
    uint16_t msgsz; \
    uint16_t msgid;

struct NODE_header {
    _NODE_header;
};

struct UM_base {
    _UM_header;
    uint8_t data[0];
};

#define UM_SKIP sizeof(struct NODE_header)
#define UM_HSIZE sizeof(struct UM_base)
#define UM_MAXDATA UM_MAXSIZE - UM_HSIZE

#define UM_DEF(um, n) \
    char um##data[n]; \
    struct UM_base* um = (void*)um##data;

#define UM_DEFFIX(type, name, id) \
    struct type name; \
    name.msgid = id; \
    name.msgsz = sizeof(name);

#define UM_DEFVAR(type, name, id) \
    char name##data[UM_MAXSIZE]; \
    struct type* name = (void*)name##data; \
    name->msgid = id; \
    name->msgsz = UM_MAXSIZE - sizeof(*name);

#define UM_CAST(type, name, um) \
    struct type* name = (struct type*)um;

struct UM_node_reg {
    _UM_header;
    uint32_t addr;
    uint16_t port;
};

struct UM_node_regok {
    _UM_header;
    uint32_t addr;
    uint16_t port;
};

struct UM_node_subs {
    _UM_header;
    uint16_t n;
    uint16_t subs[0];
};

static inline uint16_t 
UM_node_subs_size(struct UM_node_subs* um) {
    return sizeof(*um) + sizeof(um->subs[0]) * um->n;
}

struct UM_node_notify {
    _UM_header;
    uint16_t tnodeid;
    uint32_t addr;
    uint16_t port;
};

struct UM_cmd_req {
    _UM_header;
    uint32_t cid;
    char cmd[0];
};

struct UM_cmd_res {
    _UM_header;
    uint32_t cid;
    char str[0];
};

#pragma pack()

#define UM_SEND(id, um, sz) \
    (um)->nodeid = host_id(); \
    (um)->msgsz = sz; \
    host_net_send(id, um, sz);

#define UM_SENDTOCLI(id, um, sz) \
    (um)->msgsz = sz - UM_SKIP; \
    host_net_send(id, (char*)um + UM_SKIP, (um)->msgsz);

#define UM_SENDTOSVR UM_SENDTOCLI

#endif
