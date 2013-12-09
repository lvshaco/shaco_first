#ifndef __sharetype_h__
#define __sharetype_h__

#include <stdint.h>

// shaco 错误号
#define SERR_OK 0
#define SERR_UNKNOW         1
#define SERR_TIMEOUT        2
#define SERR_SOCKET         3
#define SERR_INVALIDMSG     4
#define SERR_DBERR          10
#define SERR_DBREPLY        11
#define SERR_DBREPLYTYPE    12
#define SERR_NODB           13
#define SERR_DBDATAERR      14
#define SERR_NOREGION       20
#define SERR_NOACC          21
#define SERR_ACCVERIFY      22
#define SERR_REGGATE        23
#define SERR_NOCHAR         30
#define SERR_NAMEEXIST      31
#define SERR_RELOGIN        32
#define SERR_WORLDFULL      33
#define SERR_ACCLOGINED     34
#define SERR_NOLOGIN        35
#define SERR_UNIQUECHARID   36
#define SERR_CREATECHARMUCHTIMES 37
#define SERR_MATCHFAIL      40
#define SERR_CREATEROOM     41
#define SERR_CRENOTPLT      42
#define SERR_CRENOMAP       43
#define SERR_NOROOM         50
#define SERR_ROOMKEY        51
#define SERR_NOMEMBER       52
#define SERR_ROOMOVER       53
#define SERR_ALLOC          54

#pragma pack(1)

// character
#define ACCOUNT_NAME_MAX 48
#define ACCOUNT_PASSWD_MAX 40
#define CHAR_NAME_MAX 32
#define ROLE_MAX 8
#define ROLE_CLOTHES_MAX 7
#define ROLE_TYPEID(roleid) ((roleid)/10-1)
#define ROLE_CLOTHID(roleid) ((roleid)%10)

// 戒指
#define RING_STACK 99
#define RING_MAX 100
#define RING_PAGE_MAX 7
#define RING_PAGE_SLOT 10
#define RING_PAGE_NAME 24
#define RING_PAGE_PRICE 1

struct ringobj {
    uint32_t ringid;
    uint8_t  stack;
};

struct ringpage {
    char name[RING_PAGE_NAME];
    uint32_t slots[RING_PAGE_SLOT];
};

struct ringdata {
    uint8_t npage;
    struct ringpage pages[RING_PAGE_MAX];
    uint8_t nring;
    struct ringobj  rings[RING_MAX];
};


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
    uint32_t skin;    // 使用的服装 (废弃)
    uint32_t oxygen;  // 氧气
    uint32_t body;    // 体能
    uint32_t quick;   // 敏捷

    uint8_t  ownrole[ROLE_MAX]; // 拥有的角色
    struct   ringdata ringdata; // 戒指信息
};

// room type
#define ROOM_TYPE1 1
#define ROOM_TYPE2 2
#define ROOM_LOAD_TIMELEAST 5
#define MEMBER_MAX 8

// 道具类型
#define ITEM_T_OXYGEN 1 // 氧气
#define ITEM_T_FIGHT  2 // 战斗
#define ITEM_T_TRAP   3 // 机关
#define ITEM_T_BAO    4 // 秘石
#define ITEM_T_RES    5 // 资源
#define ITEM_T_EQUIP  6 // 装备

// 道具目标类型
#define ITEM_TARGET_SELF  0
#define ITEM_TARGET_ENEMY 1
#define ITEM_TARGET_ALL 2

// 道具效果类型
//#define ITEM_EFFECT_CREATE_BAO  1
#define ITEM_EFFECT_SPEED 2
#define ITEM_EFFECT_OXYGEN 3
#define ITEM_EFFECT_PROTECT 4
#define ITEM_EFFECT_MAX 100 // > 100 为按百分比计算

// 关卡信息
struct groundattri {
    uint32_t randseed;
    uint32_t mapid;        // 地图ID
    int32_t difficulty;    // 难度
    int32_t shaketime;     // 欲坠时间
    float   cellfallspeed; // 坠落速度
    int32_t waitdestroy;   // 等待销毁时间
    int32_t destroytime;   // 销毁时间
};

// team member detail info
struct tmemberdetail {
    uint32_t charid;
    char name[CHAR_NAME_MAX];

    uint32_t role;
    uint32_t skin;
    int32_t oxygen;
    int32_t oxygencur; 
    int32_t body;
    int32_t bodycur;
    int32_t quick;
    int32_t quickcur;

    float movespeed;     // 移动速度
    float charfallspeed; // 坠落速度
    float jmpspeed;      // 跳跃速度
    int32_t jmpacctime;  // 跳跃准备时间
    int32_t rebirthtime; // 复活时间
    float dodgedistance; // 闪避距离

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

struct tmemberstat {
    uint32_t charid;
    int16_t depth;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t nbao;
};

#pragma pack()

#endif
