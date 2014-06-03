#ifndef __msg_sharetype_h__
#define __msg_sharetype_h__

#include <stdint.h>
#include <stddef.h>
#include "msg_char_attribute.h"

// shaco 错误号
#define SERR_OKUNFORCE      -1
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
#define SERR_REQGATE        15
#define SERR_UNIQUEACCID    20
#define SERR_NOACC          21
#define SERR_ACCVERIFY      22
#define SERR_ACCCREATE      23
#define SERR_ACCEXIST       24
#define SERR_GATEFULL       25

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
#define SERR_CREMAP         44
#define SERR_ROOMIDCONFLICT 45
#define SERR_PLAYCANCEL     46

#define SERR_ROOMACCLOGINED 49
#define SERR_NOROOMJOIN     50
#define SERR_NOROOMLOGIN    51
#define SERR_NOMEMBER       52
#define SERR_ROOMOVER       53
#define SERR_ALLOC          54
#define SERR_NOAUTHS        55
#define SERR_NOUNIQUEOL     56
#define SERR_ACCINSERT      57 // the same as SERR_ACCLOGINED
#define SERR_NOHALLS        58
#define SERR_NOROOMS        59
#define SERR_JOINROOM       60
#define SERR_ROOMFULL       61
#define SERR_ROOMUNJOINABLE 62
#define SERR_ROOMUNEXIST    63

#define SERR_BUGLEN         99
// handle exit
#define SERR_GATEEXIT       100
#define SERR_HALLEXIT       101
#define SERR_AUTHEXIT       102
#define SERR_ROOMEXIT       103
#define SERR_GATEHANDLEEXIT 104 // if in gate node, then = WATCHDOG EXIT
#define SERR_WATCHDOGEXIT   105
#define SERR_ROBOTEXIT      106
#define SERR_MATCHEXIT      107
#define SERR_UNIQUEOLEXIT   108

#pragma pack(1)

// field update,  the filed is 4 bytes
#define field_update(type, member, container) do { \
    int start = offsetof(struct type, member)/4; \
    int i = start >> 3; \
    int b = start & 7; \
    assert(i>=0 && i<FLAG_MAX); \
    container->flags[i] |= 1<<b; \
} while (0)

// common
#define IP_LEN 40

// character
#define ACCOUNT_NAME_MAX 48
#define ACCOUNT_PASSWD_MAX 40
#define CHAR_NAME_MAX 32 
#define ROLE_MAX 8
#define ROLE_CLOTHES_MAX 7
#define ROLE_TYPEID(roleid) ((roleid)/10-1)
#define ROLE_CLOTHID(roleid) ((roleid)%10)
#define ROLE_ID(typeid, clothid) (((typeid)+1) * 10 + (clothid))a

#define IS_VALID_TYPEID(id) ((id) >= 0 && (id) < ROLE_MAX)
#define IS_VALID_CLOTHID(id) ((id) >= 0 && (id) < ROLE_CLOTHES_MAX)

#define LEVEL_MAX 135

// 角色状态
#define ROLE_STATE_0 0 // 低迷
#define ROLE_STATE_1 1 // 疲惫
#define ROLE_STATE_2 2 // 正常
#define ROLE_STATE_3 3 // 神清气爽
#define ROLE_STATE_4 4 // 福星高照
#define ROLE_STATE_MAX 5

#define ROLE_STATE_NORMAL ROLE_STATE_3

#define STATE_0_VALUE 8
#define STATE_1_VALUE 29
#define STATE_2_VALUE 50
#define STATE_3_VALUE 81
#define STATE_4_VALUE 90
#define STATE_MAX_VALUE STATE_4_VALUE
#define STATE_LESSNORMAL_MAX_VALUE STATE_1_VALUE
#define STATE_INIT_VALUE 40

static inline int
role_state_id(int value) {
    if (value <= STATE_0_VALUE) {
        return ROLE_STATE_0;
    } else if (value <= STATE_1_VALUE) {
        return ROLE_STATE_1;
    } else if (value <= STATE_2_VALUE) {
        return ROLE_STATE_2;
    } else if (value <= STATE_3_VALUE) {
        return ROLE_STATE_3;
    } else {
        return ROLE_STATE_4;
    }
}

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

#define EFFECT_STATE            50
#define EFFECT_PROTECT          51
#define EFFECT_SPECEFFECT       52
#define EFFECT_CHANGE_CELL      53
#define EFFECT_DESTORY_CELL     54
#define EFFECT_REBIRTH          55
#define EFFECT_REVERSE          56
#define EFFECT_SEX              57

#define EFFECT_STATE_PROTECT    1 // 压不坏的防护罩
#define EFFECT_STATE_PROTECT_ONCE 2 // 一次性防护罩 

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

    uint8_t role;    // 使用的角色

    uint32_t score_normal;  // 比赛得分
    uint32_t score_dashi;   // 大师赛积分
    struct   char_attribute attri;
    uint8_t  ownrole[ROLE_MAX]; // 拥有的角色
    struct   ringdata ringdata; // 戒指信息

    float luck_factor; // 幸运系数
    uint32_t last_washgold_refresh_time; // 上次淘金库存时间
    uint32_t washgold;  // 淘金库存

    uint8_t  roles_state[ROLE_MAX]; // 角色状态
    uint32_t last_state_refresh_time; // 上次状态回复时间

    uint32_t first_time_flag;
};

static inline uint8_t *
role_state(struct chardata *cdata) {
    uint8_t type_id = ROLE_TYPEID(cdata->role);
    return &cdata->roles_state[type_id < ROLE_MAX ? type_id : 0];
}

// room type
#define ROOM_TYPE_NORMAL 0 // 普通赛, 自由/合作模式
#define ROOM_TYPE_DASHI  1 // 大师赛
#define ROOM_TYPE_MAX    2
#define ROOM_TYPE_VALID(type) ((type) >= ROOM_TYPE_NORMAL && (type) < ROOM_TYPE_MAX)

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
#define ITEM_TARGET_FRIEND 2
#define ITEM_TARGET_ALL 3

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
    uint32_t accid;
    uint32_t charid;
    char name[CHAR_NAME_MAX];

    uint16_t level;
    uint8_t role;
    uint8_t state;
    uint32_t score_dashi; // 大师赛积分
    struct char_attribute attri;
};

// team member brief info
struct tmemberbrief {
    uint32_t accid;
    uint32_t charid;
    char name[CHAR_NAME_MAX];
    uint16_t level;
    uint8_t role;
    uint8_t state;
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
    int32_t coin_extra;
    int32_t coin;
    int32_t score_display;
    int32_t score;
};

// !!!first time, do not change the no., see FIRST_FIELDS
#define FT_ACCOUNT 0
#define FT_CREATE_CHAR 1
#define FT_ENTER_HALL 2
#define FT_PLAY_GAME 3
#define FT_FREEDOM_PLAY 4
#define FT_FREEDOM_OVER 5
#define FT_DASHI_PLAY 6
#define FT_DASHI_MATCH 7
#define FT_DASHI_OVER 8
#define FT_ROLE_USE 9
#define FT_ROLE_BUY_CLICK 10
#define FT_ROLE_BUY_OK 11
#define FT_SETTING_CLICK 12
#define FT_HELP_CLICK 13
#define FT_NEWBIE_CLICK 14
#define FT_C_CLICK 15
#define FT_RANK_CLICK 16
#define FT_SCORE_CLICK 17
#define FT_DASHI_CLICK 18
#define FT_WASHGOLD 19
#define FT_ADJUSTSTATE 20
#define FT_EXCHANGE 21
#define FT_RETURN_GROUND 22
#define FT_MAX 23

#pragma pack()

#endif
