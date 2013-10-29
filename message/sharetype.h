#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

#pragma pack(1)

// character
#define CHAR_NAME_MAX 32

// 玩家信息
struct chardata {
    uint32_t charid;
    char name[CHAR_NAME_MAX];
    uint16_t level;   // 当前等级
    uint32_t exp;     // 当前等级经验

    uint32_t coin;    // 金币
    uint32_t diamond; // 钻石
    uint16_t package; // 包裹容量

    uint32_t role;    // 使用的角色
    uint32_t skin;    // 使用的服装
    uint32_t oxygen;  // 氧气
    uint32_t body;    // 体能
    uint32_t quick;   // 敏捷
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

    uint32_t role;
    uint32_t skin;
    uint32_t oxygen; 
    uint32_t body;
    uint32_t quick;
};

// team member brief info
struct tmemberbrief {
    uint32_t charid;
    char name[CHAR_NAME_MAX];

    uint32_t role;
    uint32_t skin;
    uint32_t oxygen; 
    uint32_t body;
    uint32_t quick;
};

#pragma pack()

#endif
