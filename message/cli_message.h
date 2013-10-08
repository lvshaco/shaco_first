#ifndef __cli_message_h__
#define __cil_message_h__

#include "message.h"
#include "sharetype.h"

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

struct UM_charinfo {
    _UM_header;
    struct chardata data;
};

//////////////////////////////////////////////////////////////
// play
struct UM_play {
    _UM_header;
    int8_t type; // see ROOM_TYPE*
};

struct UM_playfail {
    _UM_header;
    int8_t error;
};

struct UM_playwait {
    _UM_header;
    int timeout;
};

#define PLAY_LOADING_TIMEOUT 10
struct UM_playloading {
    _UM_header;
    int8_t leasttime;  // least time of loading
    struct tmember_brief member;
};

struct UM_playbegin {
    _UM_header;
    struct tmember_detail member;
};

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
