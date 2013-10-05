#ifndef __cli_message_h__
#define __cil_message_h__

#include "message.h"

// UMID
#define UMID_CBEGIN 1000
#define UMID_CEND   2000

#define UMID_LOGIN          UMID_CBEGIN
#define UMID_LOGINFAIL      UMID_CBEGIN+1
#define UMID_LOGOUT         UMID_CBEGIN+2
#define UMID_CHARINFO       UMID_CBEGIN+5

#define UMID_PLAY           UMID_CBEGIN+100
#define UMID_PLAYFAIL       UMID_CBEGIN+101
#define UMID_PLAYWAIT       UMID_CBEGIN+102
#define UMID_PLAYLOADING    UMID_CBEGIN+103
#define UMID_PLAYBEGIN      UMID_CBEGIN+104
#define UMID_PLAYJOIN       UMID_CBEGIN+105
#define UMID_PLAYUNJOIN     UMID_CBEGIN+106
#define UMID_PLAYDONE       UMID_CBEGIN+107

// const value
#define NAME_MAX 32

//////////////////////////////////////////////////////////////
// login
struct UM_login {
};

struct UM_loginfail {
    int error;
};

#define LOGOUT_NORMAL 0
#define LOGOUT_TIMEOUT 1
#define LOGOUT_SOCKERR 2
struct UM_logout {
    int8_t type;
};

struct UM_charinfo {
    char name[NAME_MAX];
};

//////////////////////////////////////////////////////////////
// play
struct UM_play {
    int type;
};

struct UM_playfail {
    int error;
};

struct UM_playwait {
    int timeout;
};

// team member brief info
struct tmember_brief {
    uint32_t charid;
    char name[NAME_MAX];
};
#define PLAY_LOADING_TIMEOUT 10
struct UM_playloading {
    int leasttime;  // least time of loading
    int self;
    int other;
    struct tmember_brief members[0];
};
static inline uint16_t
UM_playloading_size(struct UM_playloading* um) {
    return sizeof(*um) + sizeof(um->members[0]) * (um->self + um->other); 
}

// team member detail info
struct tmember_detail {
    uint32_t charid;
};

struct UM_playbegin {
    int count;
    struct tmember_detail members[0];
};
static inline uint16_t
UM_playbegin_size(struct UM_playbegin* um) {
    return sizeof(*um) + sizeof(um->members[0]) * um->count; 
}

struct UM_playjoin {
    struct tmember_detail member;
};

enum PLAY_UNJOIN_T {
    PUNJOIN_LEAVE = 1,
    PUNJOIN_OVER = 2,
};

struct UM_playunjoin {
    uint32_t charid;
    int reason; // enum PLAY_UNJOIN_T
};

struct UM_playdone {
};

#endif
