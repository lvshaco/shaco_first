#ifndef __user_message_h__
#define __user_message_h__

#include "message.h"
#include "sharetype.h"
#include "host_net.h"
#include "host_node.h"

#define IDUM_NBEGIN 0
#define IDUM_NEND   999 

#define IDUM_NODEREG    IDUM_NBEGIN+1
#define IDUM_NODEREGOK  IDUM_NBEGIN+2
#define IDUM_NODESUBS   IDUM_NBEGIN+3
#define IDUM_NODENOTIFY IDUM_NBEGIN+4

#define IDUM_CMDREQ     IDUM_NBEGIN+10
#define IDUM_CMDRES     IDUM_NBEGIN+11
#define IDUM_FORWARD    IDUM_NBEGIN+12

#define IDUM_CREATEROOM     IDUM_NBEGIN+200
#define IDUM_CREATEROOMRES  IDUM_NBEGIN+201
#define IDUM_OVERROOM       IDUM_NBEGIN+202

#pragma pack(1)

// node
struct UM_NODEREG {
    _UM_HEADER;
    uint32_t addr;
    uint16_t port;
    uint32_t gaddr;
    uint16_t gport;
};
struct UM_NODEREGOK {
    _UM_HEADER;
    uint32_t addr;
    uint16_t port;
    uint32_t gaddr;
    uint16_t gport;
};
struct UM_NODESUBS {
    _UM_HEADER;
    uint16_t n;
    uint16_t subs[0];
};
static inline uint16_t 
UM_NODESUBS_size(struct UM_NODESUBS* um) {
    return sizeof(*um) + sizeof(um->subs[0]) * um->n;
}
struct UM_NODENOTIFY {
    _UM_HEADER;
    uint16_t tnodeid;
    uint32_t addr;
    uint16_t port;
};

// cmd
struct UM_CMDREQ {
    _UM_HEADER;
    int32_t cid;
    char cmd[0];
};
struct UM_CMDRES {
    _UM_HEADER;
    int32_t cid;
    char str[0];
};

// forward
struct UM_FORWARD {
    _UM_HEADER;
    int32_t cid;
    struct UM_BASE wrap;
};
static inline uint16_t
UM_FORWARD_size(struct UM_FORWARD* um) {
    return sizeof(*um) + um->wrap.msgsz - UM_HSIZE;
}
#define UM_CLIMAX (UM_MAXSIZE-sizeof(struct UM_FORWARD)+UM_HSIZE)
#define UM_DEFFORWARD(fw, fid, type, name) \
    UM_DEFVAR(UM_FORWARD, fw); \
    fw->cid = fid; \
    UM_CAST(type, name, &fw->wrap); \
    name->nodeid = 0; \
    name->msgid = ID##type; \
    name->msgsz = sizeof(*name);

// room
struct UM_CREATEROOM {
    _UM_HEADER; 
    int8_t type;  // see ROOM_TYPE*
    int id;
    uint32_t key; // key of room
    int8_t nmember;
    struct tmemberdetail members[0];
};
static inline uint16_t 
UM_CREATEROOM_size(struct UM_CREATEROOM* cr) {
    return sizeof(*cr) + sizeof(cr->members[0]) * cr->nmember;
}

struct UM_CREATEROOMRES {
    _UM_HEADER;
    int8_t ok;
    int id;
    uint32_t key;
    int roomid;
};

struct UM_OVERROOM {
    _UM_HEADER;
    int8_t type;
};

#pragma pack()

#define UM_SEND(id, um, sz) do { \
    (um)->nodeid = host_id();   \
    (um)->msgsz = sz;           \
    host_net_send(id, um, sz);  \
} while(0)

#define UM_SENDTOCLI(id, um, sz) do { \
    (um)->msgsz = sz - UM_SKIP;                         \
    host_net_send(id, (char*)um + UM_SKIP, (um)->msgsz);\
} while(0)

#define UM_SENDTOSVR UM_SENDTOCLI

#define UM_SENDTONODE(hn, um, sz) \
    UM_SEND(hn->connid, um, sz)

#define UM_SENDTONID(tid, sid, um, sz) do { \
    uint16_t id = HNODE_ID(tid, sid);               \
    const struct host_node* hn = host_node_get(id); \
    if (hn) {                                       \
        UM_SENDTONODE(hn, um, sz);                  \
    }                                               \
} while(0)

#define UM_SENDFORWARD(id, fw) \
    UM_SEND(id, fw, UM_FORWARD_size(fw));

#endif
