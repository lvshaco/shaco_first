#ifndef __message_h__
#define __message_h__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define UM_MINSZ 1024
#define UM_MAXSZ 61000
#define UM_CLI_MAXSZ (UM_MAXSZ - 1000)
#define IDUM_MAX 65536
#define IDUM_INVALID -1

#pragma pack(1)

#define _UM_HEADER \
union { \
    struct { uint16_t head; }; \
    uint16_t msgid; \
}; \
uint8_t body[0];

struct UM_BASE {
    _UM_HEADER;
};

//#define UM_BASE_SZ sizeof(struct UM_BASE)
//#define UM_CLI_OFF offsetof(struct UM_BASE, cli_base)
//#define UM_CLI_BASE_SZ sizeof(struct UM_CLI_BASE)
//#define UM_CLI_SZ(um) ((um)->msgsz - UM_CLI_OFF)

//#define UM_MAXDATA UM_MAXSZ - UM_BASE_SZ
#define UM_DEF(um, n) \
    char um##data[n]; \
    struct UM_BASE* um = (void*)um##data;

#define UM_DEFFIX(type, name) \
    struct type name##data; \
    struct type* name = &name##data; \
    name->msgid = ID##type; \

#define UM_DEFVAR(type, name) \
    char name##data[UM_MAXSZ]; \
    struct type* name = (void*)name##data; \
    name->msgid = ID##type; \

#define UM_DEFVAR2(type, name, sz) \
    char name##data[sz]; \
    struct type* name = (void*)name##data; \
    name->msgid = ID##type; \

#define UM_DEFWRAP(type, name, wraptype, wrapname) \
    UM_DEFVAR2(type, name, sizeof(struct type)+sizeof(struct wraptype)) \
    struct wraptype *wrapname = (struct wraptype*)(name->wrap);

#define UM_DEFWRAP2(type, name, wrapsz) \
    UM_DEFVAR2(type, name, sizeof(struct type)+wrapsz)

#define UM_CAST(type, name, um) \
    struct type* name = (struct type*)um;

#define UM_CASTCK(type, name, msg, sz) \
    if (sz < sizeof(struct type)) return; \
    struct type *name = (struct type*)msg;

#define UD_CAST(type, name, data) \
    struct type *name = (struct type*)data; \
    name->msgid = ID##type;

#pragma pack()

#endif
