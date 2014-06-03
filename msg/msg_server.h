#ifndef __msg_server_h__
#define __msg_server_h__

#include "msg.h"
#include "msg_sharetype.h"
#include "sh_net.h"
#include "sh_node.h"

#define IDUM_NBEGIN 0
#define IDUM_NEND   999 

#define IDUM_SERVICEINIT    IDUM_NBEGIN+1
#define IDUM_SERVICEADD     IDUM_NBEGIN+2
#define IDUM_SERVICEDEL     IDUM_NBEGIN+3
#define IDUM_SERVICELOAD    IDUM_NBEGIN+4
#define IDUM_UPDATELOAD IDUM_NBEGIN+5
#define IDUM_NETDISCONN IDUM_NBEGIN+6
#define IDUM_NETCONNECT IDUM_NBEGIN+7

#define IDUM_GATE       IDUM_NBEGIN+10
#define IDUM_HALL       IDUM_NBEGIN+11
#define IDUM_AUTH       IDUM_NBEGIN+12
#define IDUM_ROOM       IDUM_NBEGIN+13
#define IDUM_MATCH      IDUM_NBEGIN+14
#define IDUM_BUG        IDUM_NBEGIN+15
#define IDUM_CMDS       IDUM_NBEGIN+16
#define IDUM_CLIENT     IDUM_NBEGIN+17
#define IDUM_DBRANK     IDUM_NBEGIN+18

#define IDUM_REDISQUERY IDUM_NBEGIN+20
#define IDUM_REDISREPLY IDUM_NBEGIN+21

#define IDUM_UNIQUEUSE    IDUM_NBEGIN+30
#define IDUM_UNIQUEUNUSE  IDUM_NBEGIN+31
#define IDUM_UNIQUESTATUS IDUM_NBEGIN+32
#define IDUM_UNIQUEREADY  IDUM_NBEGIN+33
#define IDUM_SYNCOK       IDUM_NBEGIN+35

#define IDUM_MINLOADBEGIN IDUM_NBEGIN+100 // minload begin
//#define IDUM_ACCOUNTLOGINREG IDUM_NBEGIN+100
//#define IDUM_ACCOUNTLOGINRES IDUM_NBEGIN+101
#define IDUM_LOGINACCOUNTOK  IDUM_NBEGIN+102

#define IDUM_ENTERHALL    IDUM_NBEGIN+110
#define IDUM_EXITHALL     IDUM_NBEGIN+111

#define IDUM_ENTERROOM    IDUM_NBEGIN+150
#define IDUM_LOGINROOM    IDUM_NBEGIN+151
#define IDUM_EXITROOM     IDUM_NBEGIN+152
#define IDUM_OVERROOM     IDUM_NBEGIN+153

#define IDUM_MINLOADEND   IDUM_NBEGIN+199 // minload end

#define IDUM_CREATEROOM     IDUM_NBEGIN+200
#define IDUM_CREATEROOMRES  IDUM_NBEGIN+201
#define IDUM_DESTROYROOM    IDUM_NBEGIN+203
#define IDUM_JOINROOM       IDUM_NBEGIN+204
#define IDUM_JOINROOMRES    IDUM_NBEGIN+205

#define IDUM_AWARDB         IDUM_NBEGIN+220
#define IDUM_GAMEAWARD      IDUM_NBEGIN+220
#define IDUM_STAT           IDUM_NBEGIN+221
#define IDUM_STATEND        IDUM_NBEGIN+222
#define IDUM_AWARDE         IDUM_NBEGIN+229

#define IDUM_APPLY          IDUM_NBEGIN+300
#define IDUM_APPLYCANCEL    IDUM_NBEGIN+301

//#define IDUM_ROBOTB         IDUM_NBEGIN+400
#define IDUM_ROBOT_PULL     IDUM_NBEGIN+401
#define IDUM_ROBOT_APPLY    IDUM_NBEGIN+402
#define IDUM_ROBOT_LOGINROOM IDUM_NBEGIN+403
//#define IDUM_ROBOTE         IDUM_NBEGIN+450

#pragma pack(1)

struct UM_DBRANK {
    _UM_HEADER;
    uint32_t charid;
    uint64_t score;
    uint8_t ltype;
    uint8_t ltype_old;
    char data[];
};
static inline uint16_t
UM_DBRANK_size(struct UM_DBRANK *um) {
    return sizeof(*um) + um->ltype + um->ltype_old;
}

struct UM_GATE {
    _UM_HEADER;
    int connid;
    uint8_t wrap[0];
};

struct UM_AUTH {
    _UM_HEADER;
    uint64_t conn;
    uint32_t wsession;
    uint8_t wrap[0];
};

struct UM_BUG {
    _UM_HEADER;
    uint32_t client;
    uint8_t wrap[0];
};

// watchdog 对此类消息转发给gate
struct UM_CLIENT {
    _UM_HEADER;
    uint32_t uid;
    uint8_t wrap[0];
};

// watchdog 对此类消息转发给hall
struct UM_HALL {
    _UM_HEADER;
    uint32_t uid;
    uint8_t wrap[0];
};

// watchdog 对此类消息转发给room
struct UM_ROOM {
    _UM_HEADER;
    uint32_t uid;
    uint8_t wrap[0];
};

struct UM_MATCH {
    _UM_HEADER;
    uint32_t uid;
    uint8_t wrap[0];
};

struct UM_NETCONNECT {
    _UM_HEADER;
    int32_t connid;
    char ip[40];
};

struct UM_NETDISCONN {
    _UM_HEADER;
    int8_t type;
    int err;
};

struct module_info {
    int32_t handle;
    char ip[40];
    uint16_t port;
    int32_t load;
};

struct UM_SERVICEADD {
    _UM_HEADER;
    struct module_info info;
};

struct UM_SERVICEINIT {
    _UM_HEADER;
    uint8_t ninfo;
    struct module_info info[0];
};
static inline uint16_t
UM_SERVICEINIT_size(struct UM_SERVICEINIT *um) {
    return sizeof(*um) + sizeof(um->info[0]) * um->ninfo;
}

struct UM_SERVICEDEL {
    _UM_HEADER;
    int32_t handle;
};

struct UM_SERVICELOAD {
    _UM_HEADER;
    int32_t handle;
    int32_t load; 
};

// unique
struct UM_UNIQUEUSE {
    _UM_HEADER;
    uint32_t id;
};

struct UM_UNIQUEUNUSE {
    _UM_HEADER;
    uint32_t id;
};

#define UNIQUE_USE_OK   1
#define UNIQUE_HAS_USED 2
struct UM_UNIQUESTATUS {
    _UM_HEADER;
    uint32_t id;
    int8_t status; // see UNIQUE_USE_OK
};

struct UM_UNIQUEREADY {
    _UM_HEADER;
};

struct UM_SYNCOK {
    _UM_HEADER;
};

// end unique

// cmd
struct UM_CMDS {
    _UM_HEADER;
    int connid;
    uint8_t wrap[0];
};

// load
struct UM_UPDATELOAD {
    _UM_HEADER;
    int value; // load value
};

#define RQUERY_REPLY 1
#define RQUERY_BACKUP 2
#define RQUERY_SHARDING 4

// redisproxy
struct UM_REDISQUERY {
    _UM_HEADER;
    uint8_t flag; // RQUERY_
    uint16_t cbsz;  // if RQUERY_SHARDING, then first 4 bytes must be the key
    char data[];
};

struct UM_REDISREPLY {
    _UM_HEADER;
    uint16_t cbsz;
    char data[];
};

// account login
struct UM_LOGINACCOUNTOK {
    _UM_HEADER;
    uint32_t accid;
};

// hall
struct UM_ENTERHALL {
    _UM_HEADER;
    uint32_t uid;
    char ip[40];
};

struct UM_EXITHALL {
    _UM_HEADER;
    uint32_t uid;
    int8_t err;
};

struct UM_ENTERROOM {
    _UM_HEADER;
    int room_handle;
    uint32_t roomid;
};

struct UM_EXITROOM {
    _UM_HEADER;
    uint32_t uid;
};

struct UM_OVERROOM {
    _UM_HEADER;
    uint32_t uid;
    int8_t err;
};

struct UM_LOGINROOM {
    _UM_HEADER;
    int room_handle;
    uint32_t roomid;
    float luck_factor;
    struct tmemberdetail detail;
};

struct match_member {
    uint8_t is_robot;
    struct tmemberbrief brief;
};

// room
struct UM_CREATEROOM {
    _UM_HEADER; 
    int8_t type;  // see ROOM_TYPE*
    uint32_t mapid;
    uint32_t id;
    int8_t max_member;
    int8_t nmember;
    struct match_member members[0];
};
static inline uint16_t 
UM_CREATEROOM_size(struct UM_CREATEROOM* cr) {
    return sizeof(*cr) + sizeof(cr->members[0]) * cr->nmember;
}

struct UM_CREATEROOMRES {
    _UM_HEADER;
    uint32_t id;
    int8_t err;
};

struct UM_JOINROOM {
    _UM_HEADER;
    uint32_t id;
    struct match_member mm;
};

struct UM_JOINROOMRES {
    _UM_HEADER;
    uint32_t id;
    uint32_t uid;
    int8_t err;
};

struct UM_DESTROYROOM {
    _UM_HEADER;
    uint32_t id;
};

struct memberaward {
    int32_t take_state;
    int32_t exp;
    int32_t coin;
    int32_t coin_extra;
    int32_t score_display;
    int32_t score_normal;
    int32_t score_dashi;
    float   luck_factor;
};

struct UM_GAMEAWARD {
    _UM_HEADER;
    int8_t type;
    struct memberaward award;
};
/*
#define ST_depth_max 0  // 最大深度
#define ST_time_max 1   // 最大时长
#define ST_score_max 2  // 最高分数
#define ST_speed_max 3  // 最高速率
#define ST_coin_max 4   // 最多货币
#define ST_bao_max 5    // 最多宝物
#define ST_depth_acc 16  // 总深度
#define ST_coin_acc 17   // 总获取金币
#define ST_oxygen_item_acc 18 // 总氧气类道具
#define ST_fight_item_acc 19  // 总战斗类道具
#define ST_bao_item_acc 20    // 总宝箱类道具
#define ST_co_acc 21          // 总合作局数
#define ST_dashi_acc 22      // 总对战局数
#define ST_dashi_win_acc 23  // 总对战胜利局数
#define ST_dashi_fail_acc 24 // 总对战失败局数
#define ST_games_acc 25 // 总游戏局数
#define ST_game_acc 26  // 总游戏时长
#define ST_online_acc 27// 总在线时长
#define ST_max 32
*/
#define ST_depth 0
#define ST_time 1
#define ST_score 2
#define ST_speed 3
#define ST_coin 4
#define ST_oxygen_item 5
#define ST_fight_item 6
#define ST_bao_item 7
#define ST_co_times 8
#define ST_dashi_times 9
#define ST_dashi_wins 10
#define ST_dashi_fails 11
#define ST_game_times 12
#define ST_online_time 13
#define ST_max 14

#define ST_B(x) (1<<(ST_##x))

#define ST_ALL_MAX \
    ST_B(depth) | \
    ST_B(time) | \
    ST_B(score) | \
    ST_B(speed) | \
    ST_B(coin) | \
    ST_B(bao_item)

#define ST_ALL_ACC \
    ST_B(game_times) | \
    ST_B(depth) | \
    ST_B(time)

#define ST_DAILY_MAX \
    ST_B(depth) | \
    ST_B(time) | \
    ST_B(score)
    
#define ST_DAILY_ACC \
    ST_B(depth) | \
    ST_B(time) | \
    ST_B(coin) | \
    ST_B(oxygen_item) | \
    ST_B(fight_item) | \
    ST_B(bao_item) | \
    ST_B(co_times) | \
    ST_B(dashi_times) | \
    ST_B(dashi_wins) | \
    ST_B(dashi_fails) | \
    ST_B(game_times) | \
    ST_B(online_time)

#define ST_WEEK_MAX \
    ST_B(depth) | \
    ST_B(time) | \
    ST_B(score)
    
#define ST_WEEK_ACC \
    ST_B(depth) | \
    ST_B(time) | \
    ST_B(coin) | \
    ST_B(oxygen_item) | \
    ST_B(fight_item) | \
    ST_B(bao_item) | \
    ST_B(co_times) | \
    ST_B(dashi_times) | \
    ST_B(dashi_wins) | \
    ST_B(dashi_fails) | \
    ST_B(game_times) | \
    ST_B(online_time)

struct UM_STAT {
    _UM_HEADER;
    uint32_t id;
    uint32_t flag;
    uint32_t data[];
};

struct UM_STATEND {
    _UM_HEADER;
    uint32_t id;
};

#define APPLY_TARGET_TYPE_NONE   0
#define APPLY_TARGET_TYPE_ROOM   1
#define APPLY_TARGET_TYPE_PLAYER 2
struct apply_target {
    int8_t type; // APPLY_TARGET_TYPE_
    uint32_t id;
};

struct apply_info {
    int8_t   type;
    uint8_t  luck_rand;
    uint32_t match_score;
    struct apply_target target;
    struct tmemberbrief brief;
};

struct UM_APPLY {
    _UM_HEADER;
    struct apply_info info;
};

struct UM_APPLYCANCEL {
    _UM_HEADER;
};

struct UM_ROBOT_PULL {
    _UM_HEADER;
    int8_t type;
    int8_t ai;
    uint32_t match_score;
    struct apply_target target;
};

struct UM_ROBOT_APPLY {
    _UM_HEADER;
    struct apply_info info;
};

struct UM_ROBOT_LOGINROOM {
    _UM_HEADER;
    uint32_t roomid;
    uint8_t ai;
    struct tmemberdetail detail;
};

#pragma pack()

#endif
