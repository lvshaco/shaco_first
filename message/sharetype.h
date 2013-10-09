#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

#pragma pack(1)

// character
#define NAME_MAX 32
struct chardata {
    uint32_t charid;
    char name[NAME_MAX];
};

// room type
#define ROOM_TYPE1 1
#define ROOM_TYPE2 2

#define MEMBER_MAX 8

// team member detail info
struct tmemberdetail {
    int32_t login:1;
    int32_t online:1;
    int32_t unused:30;
    int tagid;
    uint32_t charid;
    char name[NAME_MAX];
};

// team member brief info
struct tmemberbrief {
    uint32_t charid;
    char name[NAME_MAX];
};

#pragma pack()

#endif
