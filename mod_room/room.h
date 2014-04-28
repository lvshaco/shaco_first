#ifndef __room_h__
#define __room_h__

#include "sh_hash.h"
#include "sh_array.h"
#include "msg_sharetype.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TICK_INTV (100)
#define ROOM_UPDATE_INTV (1000)

#define SEC_TO_FLOAT_TICK(sec) ((1000.0/TICK_INTV)*sec)
#define SEC_TO_TICK(sec) (int)((SEC_TO_FLOAT_TICK(sec) < 1) ? 1 : (SEC_TO_FLOAT_TICK(sec)+0.5))
#define SEC_ELAPSED(sec) ((self->tick % SEC_TO_TICK(sec)) == 0)

// 房间模式
#define MODE_FREE  0 // 自由
#define MODE_CO    1 // 合作
#define MODE_FIGHT 2 // 对战
#define MODE_MAX 3

struct tplt;

struct room {
    int watchdog_handle;
    int match_handle;
    int robot_handle;
    struct tplt *T;
    struct sh_hash *MH; 
    int tick;
    uint32_t randseed;
    uint32_t map_randseed;
    struct sh_hash players;
    struct sh_hash room_games;
};

struct buff_delay {
    uint32_t id;
    uint64_t first_time;
    uint64_t last_time;
};

struct one_effect {
    int  type;
    bool isper;
    float value;
};

#define BUFF_EFFECT 3
struct buff_effect {
    uint32_t id;
    struct one_effect effects[BUFF_EFFECT];
    uint64_t time;
};

struct ai_brain;

struct player {
    int watchdog_source; // if isrobot then, this is robot handle
    uint8_t index;
    uint8_t team; // 所属队伍的标志
    bool logined; // 是否登录过
    bool online;  // 是否在线
    bool loadok;  // 客户端是否加载完成
    bool is_robot;// 是否机器人
    int refresh_flag;
    float luck_factor;
    struct tmemberdetail detail;
    struct char_attribute base;
    struct sh_array total_delay;
    struct sh_array total_effect;
    int32_t depth;
    uint64_t deathtime;
    int16_t noxygenitem;
    int16_t nitem;
    int16_t ntrap;
    int16_t nbao;
    int16_t nbedamage;
    float speed_new;
    float speed_old;
    uint32_t store_item1;
    uint32_t store_item2;
    struct ai_brain *brain;
};

#define is_robot(m) ((m)->is_robot)
#define is_player(m) (!(is_robot(m)))
#define is_online(m) ((m)->online)
#define is_logined(m) ((m)->logined)
#define is_offline(m) (is_logined(m) && !is_online(m))

struct luck_item {
    uint32_t luck;
    uint32_t id;
};

struct room_item {
    uint32_t luck_up;
    uint16_t n;
    struct luck_item p[10];
};

struct room_game { 
    uint32_t id;
    int8_t type; // ROOM_TYPE_
    int8_t status; // ROOMS_
    uint64_t statustime;
    uint64_t starttime;
    int8_t maxp;
    int8_t np;
    struct player p[MEMBER_MAX];
    struct room_item mode_items[MODE_MAX];
    struct room_item fight_items2;
    struct groundattri gattri;
    struct genmap* map;
};

struct member_n {
    int8_t player;
    int8_t robot;
};

int  room_online_nplayer(struct room_game *ro);
bool room_preonline_1player(struct room_game *ro);
   
static inline int
room_game_mode(struct room_game *ro) {
    if (ro->type == ROOM_TYPE_DASHI) {
        return MODE_FIGHT;
    } else {
        return (ro->np > 1) ? MODE_CO : MODE_FREE;
    }
}

static inline struct room_game *
room_member_to_game(struct player *m) {
    assert(m->index >= 0 && m->index < MEMBER_MAX);
    return (struct room_game*)((char*)m - m->index * sizeof(*m) - offsetof(struct room_game, p));
}

static inline struct player *
room_member_front(struct room_game *ro, struct player *m) {
    int i;
    for (i=0; i<ro->np; ++i) { 
        struct player *other = &ro->p[i];
        if (other != m &&
            is_online(other) &&
            other->depth > m->depth) {
            return other;
        }
    }
    return NULL;
}

#define UID(m) ((m)->detail.accid)

#endif
