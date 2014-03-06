#ifndef __room_h__
#define __room_h__

#include "sh_hash.h"
#include "sh_array.h"
#include "msg_sharetype.h"
#include <stdint.h>
#include <stdbool.h>

#define TICK_INTV (100)
#define ROOM_UPDATE_INTV (1000)

#define SEC_TO_FLOAT_TICK(sec) ((1000.0/TICK_INTV)*sec)
#define SEC_TO_TICK(sec) (int)((SEC_TO_FLOAT_TICK(sec) < 1) ? 1 : (SEC_TO_FLOAT_TICK(sec)+0.5))
#define SEC_ELAPSED(sec) ((self->tick % SEC_TO_TICK(sec)) == 0)

struct tplt;

struct room {
    struct tplt *T;
    struct sh_hash *MH; 
    //int watchdog_handle;
    //int match_handle;
    int tick;
    uint32_t randseed;
    uint32_t map_randseed;
    struct sh_hash players;
    struct sh_hash gamerooms;
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
    bool login;
    bool online;
    bool loadok;
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
    struct ai_brain *brain;
};

#define is_robot(m) ((m)->brain != NULL)

struct luck_item {
    uint32_t luck;
    uint32_t id;
};

struct room_item {
    uint32_t luck_up;
    uint16_t n;
    struct luck_item p[10];
};


struct gameroom { 
    uint32_t id;
    int8_t type; // ROOM_TYPE*
    //uint32_t key;
    int status; // ROOMS_*
    uint64_t statustime;
    uint64_t starttime;
    int np;
    struct player p[MEMBER_MAX];
    struct room_item items;
    struct groundattri gattri;
    struct genmap* map;
};

#define member2gameroom(m) ({ \
    assert(m->index >=0 && m->index < MEMBER_MAX); \
    ((struct gameroom*)((char*)(m) - (m)->index * sizeof(*(m)) - offsetof(struct gameroom, p))); \
})

#define UID(m) ((m)->detail.accid)

#endif
