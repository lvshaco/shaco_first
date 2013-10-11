#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

#pragma pack(1)

// character
#define CHAR_NAME_MAX 32
struct chardata {
    uint32_t charid;
    char name[CHAR_NAME_MAX];
};

// room type
#define ROOM_TYPE1 1
#define ROOM_TYPE2 2

#define ROOM_LOAD_TIMELEAST 3 

#define MEMBER_MAX 8

// team member detail info
struct tmemberdetail {
    uint32_t charid;
    char name[CHAR_NAME_MAX];
};

// team member brief info
struct tmemberbrief {
    uint32_t charid;
    char name[CHAR_NAME_MAX];
};

#pragma pack()

#endif
