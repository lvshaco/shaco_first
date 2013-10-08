#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

// char
#define NAME_MAX 32
struct chardata {
    uint32_t charid;
    char name[NAME_MAX];
};

// room type
#define ROOM_TYPE1 1
#define ROOM_TYPE2 2

// team member detail info
struct tmember_detail {
    uint32_t charid;
};

// team member brief info
struct tmember_brief {
    uint32_t charid;
    char name[NAME_MAX];
};

#endif
