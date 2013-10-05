#ifndef __cli_message_h__
#define __cil_message_h__

#include "message.h"

// UMID
#define UMID_CBEGIN 1000
#define UMID_CEND   2000

#define UMID_HEARTBEAT      UMID_CBEGIN
#define UMID_LOGIN          UMID_CBEGIN+1
#define UMID_LOGINFAIL      UMID_CBEGIN+2
#define UMID_LOGOUT         UMID_CBEGIN+3
#define UMID_CHARINFO       UMID_CBEGIN+4

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
////////////////////////////////////////////////////////////
// heartbeat
struct UM_heartbeat {
    _UM_header;
};

////////////////////////////////////////////////////////////
// login
struct UM_login {
    _UM_header;
};

struct UM_loginfail {
    _UM_header;
    int8_t error;
};

#define LOGOUT_NORMAL 0
#define LOGOUT_TIMEOUT 1
#define LOGOUT_SOCKERR 2
#define LOGOUT_FULL 3
#define LOGOUT_RELOGIN 4
#define LOGOUT_NOLOGIN 5

struct UM_logout {
    _UM_header;
    int8_t type;
};

struct chardata {
    uint32_t charid;
    char name[NAME_MAX];
};

struct UM_charinfo {
    _UM_header;
    struct chardata data;
};

//////////////////////////////////////////////////////////////
// play
struct UM_play {
    _UM_header;
    int8_t type;
};

struct UM_playfail {
    _UM_header;
    int8_t error;
};

struct UM_playwait {
    _UM_header;
    int timeout;
};

// team member brief info
struct tmember_brief {
    uint32_t charid;
    char name[NAME_MAX];
};
#define PLAY_LOADING_TIMEOUT 10
struct UM_playloading {
    _UM_header;
    int8_t leasttime;  // least time of loading
    int8_t self;
    int8_t other;
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
    _UM_header;
    int8_t count;
    struct tmember_detail members[0];
};
static inline uint16_t
UM_playbegin_size(struct UM_playbegin* um) {
    return sizeof(*um) + sizeof(um->members[0]) * um->count; 
}

struct UM_playjoin {
    _UM_header;
    struct tmember_detail member;
};

enum PLAY_UNJOIN_T {
    PUNJOIN_LEAVE = 1,
    PUNJOIN_OVER = 2,
};

struct UM_playunjoin {
    _UM_header;
    uint32_t charid;
    int8_t reason; // enum PLAY_UNJOIN_T
};

struct UM_playdone {
    _UM_header;
};

#endif
