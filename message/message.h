#ifndef __message_h__
#define __message_h__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define UM_MAXSZ 61000
#define UM_CLI_MAXSZ (UM_MAXSZ - 1000)
#define IDUM_MAX 65536
#define IDUM_INVALID -1

#pragma pack(1)

// nodeid: source node id where UM from
#define _NODE_HEADER \
    uint16_t nodeid;

#define _UM_CLI_HEADER \
    uint16_t msgsz; \
    uint16_t msgid;

#define _UM_HEADER \
    _NODE_HEADER; \
    _UM_CLI_HEADER;

struct NODE_HEADER {
    _NODE_HEADER;
};

struct UM_CLI_BASE {
    _UM_CLI_HEADER;
};

struct UM_BASE {
    union {
        struct { _UM_HEADER; };
        struct { 
            struct NODE_HEADER nod_head;
            struct UM_CLI_BASE cli_base;
        };
    };
    uint8_t data[0];
};

#define UM_BASE_SZ sizeof(struct UM_BASE)
#define UM_CLI_OFF offsetof(struct UM_BASE, cli_base)
#define UM_CLI_BASE_SZ sizeof(struct UM_CLI_BASE)
#define UM_CLI_SZ(um) ((um)->msgsz - UM_CLI_OFF)

//#define UM_MAXDATA UM_MAXSZ - UM_BASE_SZ
#define UM_DEF(um, n) \
    char um##data[n]; \
    struct UM_BASE* um = (void*)um##data;

#define UM_DEFFIX(type, name) \
    struct type name##data; \
    struct type* name = &name##data; \
    name->msgid = ID##type; \
    name->msgsz = sizeof(*name);

#define UM_DEFVAR(type, name) \
    char name##data[UM_MAXSZ]; \
    struct type* name = (void*)name##data; \
    name->msgid = ID##type; \
    name->msgsz = UM_MAXSZ;

#define UM_CAST(type, name, um) \
    struct type* name = (struct type*)um;

#pragma pack()

#endif
