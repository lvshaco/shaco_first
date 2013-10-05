#ifndef __user_message_h__
#define __user_message_h__

#include "message.h"
#include "host_net.h"
#include "host_node.h"

#define UMID_NBEGIN 1
#define UMID_NEND   999 

#define UMID_NODE_REG UMID_NBEGIN
#define UMID_NODE_REGOK 2
#define UMID_NODE_SUB 3
#define UMID_NODE_NOTIFY 4
#define UMID_CMD_REQ 10
#define UMID_CMD_RES 11
#define UMID_FORWARD 100

#pragma pack(1)

// node
struct UM_node_reg {
    _UM_header;
    uint32_t addr;
    uint16_t port;
    uint32_t gaddr;
    uint16_t gport;
};
struct UM_node_regok {
    _UM_header;
    uint32_t addr;
    uint16_t port;
    uint32_t gaddr;
    uint16_t gport;
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

// cmd
struct UM_cmd_req {
    _UM_header;
    int32_t cid;
    char cmd[0];
};
struct UM_cmd_res {
    _UM_header;
    int32_t cid;
    char str[0];
};

// forward
struct UM_forward {
    _UM_header;
    int32_t cid;
    struct UM_base wrap;
};
static inline uint16_t
UM_forward_size(struct UM_forward* um) {
    return sizeof(*um) + um->wrap.msgsz - UM_HSIZE;
}
#define UM_CLIMAX (UM_MAXSIZE-sizeof(struct UM_forward)+UM_HSIZE)

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
