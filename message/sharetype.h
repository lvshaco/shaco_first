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
#define SERR_NOAUTHS        55

#pragma pack(1)

// character
#define ACCOUNT_NAME_MAX 48
#define ACCOUNT_PASSWD_MAX 40
#define CHAR_NAME_MAX 32
#define ROLE_MAX 8
#define ROLE_CLOTHES_MAX 7
#define ROLE_TYPEID(roleid) ((roleid)/10-1)
#define ROLE_CLOTHID(roleid) ((roleid)%10)
#define LEVEL_MAX 135

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
    uint8_t usepage;
    uint8_t npage;
    struct ringpage pages[RING_PAGE_MAX];
    uint8_t nring;
    struct ringobj  rings[RING_MAX];
};

#define EFFECT_INVALID  0
#define EFFECT_OXYGEN           1
#define EFFECT_BODY             2
#define EFFECT_QUICK            3
#define EFFECT_COIN_PROFIT      4
#define EFFECT_MOVE_SPEED       5
#define EFFECT_FALL_SPEED       6
#define EFFECT_ATTACK_DISTANCE  7
#define EFFECT_ATTACK_RANGE     8
#define EFFECT_ATTACK_POWER     9
#define EFFECT_LUCK             10
#define EFFECT_ATTACK_SPEED     11
#define EFFECT_DODGE_DISTANCE   12
#define EFFECT_REBIRTH_TIME     13
#define EFFECT_JUMP_RANGE       14
#define EFFECT_SENCE_RANGE      15
#define EFFECT_WINCOIN_PROFIT   16
#define EFFECT_EXP_PROFIT       17
#define EFFECT_ITEM_TIME        18
#define EFFECT_ITEM_OXYGEN      19
#define EFFECT_PRICES           20
#define EFFECT_SKILL_ACTIVE     21
#define EFFECT_SKILL_PASSIVE    22
#define EFFECT_VIEW_RANGE       23
#define EFFECT_SCORE_PROFIT     24
#define EFFECT_WINSCORE_PROFIT  25

#define EFFECT_PROTECT          51
#define EFFECT_SPECEFFECT       52
#define EFFECT_CHANGE_CELL      53
#define EFFECT_DESTORY_CELL     54
#define EFFECT_REBIRTH          55
#define EFFECT_REVERSE          56
#define EFFECT_SEX              57

struct char_attribute {
    int32_t oxygen;     // 氧气
    int32_t body;       // 体能
    int32_t quick;      // 敏捷
    
    float movespeed;     // 移动速度
    float movespeedadd;
    float charfallspeed; // 坠落速度
    float charfallspeedadd;
    float jmpspeed;      // 跳跃速度--
    int32_t jmpacctime;  // 跳跃准备时间--
    int32_t rebirthtime; // 复活时间
    float rebirthtimeadd;
    float dodgedistance; // 闪避距离
    float dodgedistanceadd;
    int32_t jump_range;  // 跳跃高度
    int32_t sence_range; // 感知范围
    int32_t view_range;  // 视野范围
   
    int32_t attack_power;   // 攻击力, 攻击一下扣除的地块耐久度
    int32_t attack_distance;// 攻击距离, 离地块的最大长度
    int32_t attack_range;   // 攻击范围，攻击的格子范围
    int32_t attack_speed;   // 攻击速度毫秒

    float   coin_profit;    // 金币收益
    float   wincoin_profit; // 胜利金币收益
    float   score_profit;   // 得分收益
    float   winscore_profit;// 胜利得分收益
    float   exp_profit;     // 经验收益
    float   item_timeadd;   // 物品时长加成
    float   item_oxygenadd; // 物品氧气效果加成
    int32_t lucky;          // 幸运
    int32_t prices;         // 身价
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

    uint32_t score_normal;  // 普通赛得分
    uint32_t score_dashi;   // 大师赛积分
    struct   char_attribute attri;
    uint8_t  ownrole[ROLE_MAX]; // 拥有的角色
    struct   ringdata ringdata; // 戒指信息
};

// room type
#define ROOM_TYPE_NORMAL 0 // 普通赛
#define ROOM_TYPE_DASHI  1 // 大师赛
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
    uint32_t score_dashi; // 大师赛积分
    struct char_attribute attri;
};

// team member brief info
struct tmemberbrief {
    uint32_t charid;
    char name[CHAR_NAME_MAX];

    uint32_t role;
    uint32_t skin;
    int32_t oxygen; 
    int32_t body;
    int32_t quick;
};

struct tmemberstat {
    uint32_t charid;
    int16_t depth;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t nbao;
    int32_t exp;
    int32_t coin;
    int32_t score;
};

#pragma pack()

#endif
