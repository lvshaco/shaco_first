#ifndef __user_message_h__
#define __user_message_h__

#include "message.h"
#include "sharetype.h"
#include "sc_net.h"
#include "sc_node.h"

#define IDUM_NBEGIN 0
#define IDUM_NEND   999 

#define IDUM_SERVICEINFO    IDUM_NBEGIN+1
#define IDUM_SERVICEDEL     IDUM_NBEGIN+2
#define IDUM_SERVICELOAD    IDUM_NBEGIN+3
#define IDUM_HALL       IDUM_NBEGIN+4
#define IDUM_NETDISCONN IDUM_NBEGIN+5
#define IDUM_GATE       IDUM_NBEGIN+6
#define IDUM_CLOSECONN  IDUM_NBEGIN+7
#define IDUM_AUTH       IDUM_NBEGIN+8
#define IDUM_ROOM       IDUM_NBEGIN+9

#define IDUM_CMDREQ     IDUM_NBEGIN+10
#define IDUM_CMDRES     IDUM_NBEGIN+11
#define IDUM_FORWARD    IDUM_NBEGIN+12
#define IDUM_MINLOADFAIL IDUM_NBEGIN+13
#define IDUM_UPDATELOAD IDUM_NBEGIN+14

#define IDUM_REDISQUERY IDUM_NBEGIN+20
#define IDUM_REDISREPLY IDUM_NBEGIN+21

#define IDUM_UNIQUEUSE    IDUM_NBEGIN+30
#define IDUM_UNIQUEUNUSE  IDUM_NBEGIN+31
#define IDUM_UNIQUESTATUS IDUM_NBEGIN+32

#define IDUM_MINLOADBEGIN IDUM_NBEGIN+100 // minload begin
//#define IDUM_ACCOUNTLOGINREG IDUM_NBEGIN+100
//#define IDUM_ACCOUNTLOGINRES IDUM_NBEGIN+101
#define IDUM_LOGINACCOUNTOK  IDUM_NBEGIN+102
#define IDUM_ENTERHALL    IDUM_NBEGIN+110

#define IDUM_MINLOADEND   IDUM_NBEGIN+199 // minload end

#define IDUM_CREATEROOM     IDUM_NBEGIN+200
#define IDUM_CREATEROOMRES  IDUM_NBEGIN+201
#define IDUM_OVERROOM       IDUM_NBEGIN+202

#pragma pack(1)

struct UM_GATE {
    _UM_HEADER;
    int connid;
    uint8_t wrap[0];
};

struct UM_AUTH {
    _UM_HEADER;
    uint64_t conn;
    uint32_t wsession;
    uint8_t wrap[0];
};

struct UM_HALL {
    _UM_HEADER;
    uint32_t accid;
    uint8_t wrap[0];
};

struct UM_ROOM {
    _UM_HEADER;
    uint32_t accid;
    uint8_t wrap[0];
};

struct UM_NETDISCONN {
    _UM_HEADER;
    int8_t err;
};

struct UM_CLOSECONN {
    _UM_HEADER;
    int8_t force; // !=0 for force
};

struct service_info {
    int32_t handle;
    char ip[40];
    uint16_t port;
    int32_t load;
};

struct UM_SERVICEINFO {
    _UM_HEADER;
    uint8_t ninfo;
    struct service_info info[0];
};
static inline uint16_t
UM_SERVICEINFO_size(struct UM_SERVICEINFO *um) {
    return sizeof(*um) + sizeof(um->info[0]) * um->ninfo;
}

struct UM_SERVICEDEL {
    _UM_HEADER;
    int32_t handle;
};

struct UM_SERVICELOAD {
    _UM_HEADER;
    int32_t handle;
    int32_t load; 
};

// unique
struct UM_UNIQUEUSE {
    _UM_HEADER;
    uint32_t id;
};

struct UM_UNIQUEUNUSE {
    _UM_HEADER;
    uint32_t id;
};

#define UNIQUE_USE_OK   1
#define UNIQUE_HAS_USED 2
struct UM_UNIQUESTATUS {
    _UM_HEADER;
    uint32_t id;
    int8_t status; // see UNIQUE_USE_OK
};
// end unique

/*
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
*/

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
/*
// forward
struct UM_FORWARD {
    _UM_HEADER;
    int32_t cid;
    struct UM_BASE wrap;
};
static inline uint16_t
UM_FORWARD_size(struct UM_FORWARD* um) {
    return sizeof(*um) + um->wrap.msgsz - UM_BASE_SZ;
}
//#define UM_CLI_MAXSZ (UM_MAXSZ-sizeof(struct UM_FORWARD)+UM_BASE_SZ)
#define UM_DEFFORWARD(fw, fid, type, name) \
    UM_DEFVAR(UM_FORWARD, fw); \
    fw->cid = fid; \
    UM_CAST(type, name, &fw->wrap); \
    name->nodeid = 0; \
    name->msgid = ID##type; \
    name->msgsz = sizeof(*name);
*/
// load
struct UM_UPDATELOAD {
    _UM_HEADER;
    int value; // load value
};
struct UM_MINLOADFAIL {
    _UM_HEADER;
};

// redisproxy
struct UM_REDISQUERY {
    _UM_HEADER;
    uint16_t needreply:1;
    uint16_t needrecord:1;
    uint16_t cbsz:14; 
    char data[];
};

struct UM_REDISREPLY {
    _UM_HEADER;
    uint16_t cbsz;
    char data[];
};

// account login
/*
struct UM_ACCOUNTLOGINREG {
    _UM_HEADER;
    int32_t cid;
    uint32_t accid;
    uint64_t key;
    uint32_t clientip;
    char account[ACCOUNT_NAME_MAX];
};
struct UM_ACCOUNTLOGINRES {
    _UM_HEADER;
    int8_t ok;
    int32_t cid;
    uint32_t accid;
    uint64_t key;
    uint32_t addr;
    uint16_t port;
};
*/
struct UM_LOGINACCOUNTOK {
    _UM_HEADER;
    uint32_t accid;
};

// hall
struct UM_ENTERHALL {
    _UM_HEADER;
    uint32_t accid;
};

struct UM_ENTERROOM {
    _UM_HEADER;
    uint16_t room_handle;
};

// room
struct UM_CREATEROOM {
    _UM_HEADER; 
    int8_t type;  // see ROOM_TYPE*
    uint32_t mapid;
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
    int32_t error;
    int32_t id;
    uint32_t key;
    int32_t roomid;
};

struct memberaward {
    uint32_t charid;
    int32_t exp;
    int32_t coin;
    int32_t score;
};

struct UM_OVERROOM {
    _UM_HEADER;
    int8_t type;
    int8_t nmember;
    struct memberaward awards[0];
};
static inline uint16_t
UM_OVERROOM_size(struct UM_OVERROOM* um) {
    return sizeof(*um) + sizeof(um->awards[0]) * um->nmember;
}

#pragma pack()

#define UM_SEND(id, um, sz) do { \
    (um)->nodeid = sc_id();   \
    (um)->msgsz = sz;           \
    sc_net_send(id, um, sz);  \
} while(0)

#define UM_SENDTOCLI(id, um, sz) do { \
    (um)->msgsz = sz; \
    sc_net_send(id, (char*)um + UM_CLI_OFF, (um)->msgsz - UM_CLI_OFF); \
} while(0)

#define UM_SENDTOSVR UM_SENDTOCLI

#define UM_SENDTONODE(hn, um, sz) \
    UM_SEND(hn->connid, um, sz)

#define UM_SENDTONID(tid, sid, um, sz) do { \
    uint16_t id = HNODE_ID(tid, sid);               \
    const struct sc_node* hn = sc_node_get(id); \
    if (hn) {                                       \
        UM_SENDTONODE(hn, um, sz);                  \
    }                                               \
} while(0)

#define UM_SENDFORWARD(id, fw) \
    UM_SEND(id, fw, UM_FORWARD_size(fw));

#endif
