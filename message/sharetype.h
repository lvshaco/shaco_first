#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

// shaco 错误号
#define SERR_OK 0
#define SERR_UNKNOW 1
#define SERR_TIMEOUT 2
#define SERR_SOCKET 3
#define SERR_DBERR 10
#define SERR_DBREPLY 11
#define SERR_DBREPLYTYPE 12
#define SERR_NODB 13
#define SERR_NOCHAR 20
#define SERR_NAMEEXIST 21
#define SERR_RELOGIN 30
#define SERR_WORLDFULL 31
#define SERR_ACCLOGINED 32
#define SERR_NOLOGIN 33


#pragma pack(1)

// character
#define ACCOUNT_NAME_MAX 48
#define ACCOUNT_PASSWD_MAX 40
#define CHAR_NAME_MAX 32

// 玩家信息
struct chardata {
    uint32_t charid;
    char name[CHAR_NAME_MAX];
    uint32_t accid;   // 账号ID
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
