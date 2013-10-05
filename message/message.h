#ifndef __message_h__
#define __message_h__

#include <stdint.h>
#include <stdlib.h>

#define UM_MAXSIZE 60000
#define UMID_MAX 65536
#define UMID_INVALID -1

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
//#define UM_MAXDATA UM_MAXSIZE - UM_HSIZE
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
    name->msgsz = UM_MAXSIZE;

#define UM_CAST(type, name, um) \
    struct type* name = (struct type*)um;

#pragma pack()

#endif
