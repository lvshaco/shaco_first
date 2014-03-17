#ifndef __match_h__
#define __match_h__

#include "msg_sharetype.h"
#include "sh_hash.h"
#include <stdint.h>
#include <stdbool.h>

// 
#define WAIT_TIMEOUT 3
#define JOINABLE_TIME 3

//
#define S_WAITING 0
#define S_CREATING 1
#define S_GAMING 2

#define N_0 0
#define N_1 1
#define N_2 2
#define N_3 3
#define N_4 4
#define N_MAX 5

#define S_MAX 65

struct applyer {
    int hall_source; // if is_robot is true, then this is robot_handle
    uint32_t uid;
    bool is_robot;
    int8_t type;
    int8_t status;
    uint8_t luck_rand; 
    uint32_t match_score;
    uint32_t match_slot;
    struct tmemberbrief brief;
    uint32_t roomid;
};

struct member {
    bool is_robot;
    uint32_t uid;
};

struct room {
    uint32_t id;
    uint64_t start_time;
    uint32_t autopull_time;
    int room_handle;
    int joinable;
    int autoswitch;
    uint32_t match_score;
    uint32_t match_slot;
    int8_t type;
    int8_t status;
    uint8_t nmember;
    struct member members[MEMBER_MAX];
};

struct waiter {
    bool is_robot;
    uint32_t uid;
    uint64_t waiting_timeout;
};

struct match {
    int hall_handle;
    int room_handle;
    int robot_handle;
    uint32_t roomid_alloctor;
    uint32_t randseed;
    struct waiter waiting_S[S_MAX];
    struct sh_hash applyers;
    struct sh_hash rooms;
    struct sh_hash joinable_rooms[N_MAX];
};

static inline uint32_t
match_score_to_N(uint32_t score) {
    if (score < 100000) {
        return N_0;
    } else if (score < 180000) {
        return N_1;
    } else if (score < 250000) {
        return N_2;
    } else if (score < 310000) {
        return N_3;
    } else {
        return N_4;
    }
}

static inline uint32_t
match_score_to_S(uint32_t score) {
    uint32_t slot = score/150;
    return slot < S_MAX ? slot : (S_MAX-1);
}

static inline int
match_score_to_Nai(uint32_t score) {
    static int TO_AI[N_MAX] = {4,5,6,7,8};
    uint32_t slot = match_score_to_N(score);
    return TO_AI[slot]; 
}

static inline int
match_score_to_Sai(uint32_t score) {
    int ai = score / 900 + 1; 
    return ai <= 10 ? ai : 10;
}

static inline uint32_t
match_slot(int type, uint32_t score) {
    if (type == ROOM_TYPE_NORMAL)
        return match_score_to_N(score);
    else
        return match_score_to_S(score);
}

static inline int
match_ai(int type, uint32_t score) {
    if (type == ROOM_TYPE_NORMAL)
        return match_score_to_Nai(score);
    else
        return match_score_to_Sai(score);
}

#endif
