#ifndef __cli_message_h__
#define __cli_message_h__

#include "message.h"
#include "sharetype.h"

// IDUM
#define IDUM_CBEGIN 1000
#define IDUM_CEND   2000

#define IDUM_HEARTBEAT      IDUM_CBEGIN
#define IDUM_LOGIN          IDUM_CBEGIN+1
#define IDUM_LOGINFAIL      IDUM_CBEGIN+2
#define IDUM_LOGOUT         IDUM_CBEGIN+3
#define IDUM_CHARINFO       IDUM_CBEGIN+4

#define IDUM_PLAY           IDUM_CBEGIN+100
#define IDUM_PLAYfail       IDUM_CBEGIN+101
#define IDUM_PLAYWAIT       IDUM_CBEGIN+102
#define IDUM_PLAYLOADING    IDUM_CBEGIN+103
#define IDUM_PLAYbegin      IDUM_CBEGIN+104
#define IDUM_PLAYjoin       IDUM_CBEGIN+105
#define IDUM_PLAYunjoin     IDUM_CBEGIN+106
#define IDUM_PLAYdone       IDUM_CBEGIN+107

#define IDUM_NOTIFYGAME     IDUM_CBEGIN+200
#define IDUM_GAMELOGIN      IDUM_CBEGIN+201
#define IDUM_GAMELOGINFAIL  IDUM_CBEGIN+202
#define IDUM_GAMELOGOUT     IDUM_CBEGIN+203
#define IDUM_GAMEINFO       IDUM_CBEGIN+204
#define IDUM_GAMEENTER      IDUM_CBEGIN+205
#define IDUM_GAMESTART      IDUM_CBEGIN+206

#pragma pack(1)
////////////////////////////////////////////////////////////
// heartbeat
struct UM_HEARTBEAT {
    _UM_HEADER;
};

struct UM_NOTIFYGAME {
    _UM_HEADER;
    uint32_t addr;
    uint16_t port;
    uint32_t key;
};

////////////////////////////////////////////////////////////
// login
struct UM_LOGIN {
    _UM_HEADER;
};

struct UM_LOGINFAIL {
    _UM_HEADER;
    int8_t error;
};

#define LOGOUT_NORMAL 0
#define LOGOUT_TIMEOUT 1
#define LOGOUT_SOCKERR 2
#define LOGOUT_FULL 3
#define LOGOUT_RELOGIN 4
#define LOGOUT_NOLOGIN 5

struct UM_LOGOUT {
    _UM_HEADER;
    int8_t type;
};

struct UM_CHARINFO {
    _UM_HEADER;
    struct chardata data;
};

//////////////////////////////////////////////////////////////
// play
struct UM_PLAY {
    _UM_HEADER;
    int8_t type; // see ROOM_TYPE*
};

struct UM_PLAYfail {
    _UM_HEADER;
    int8_t error;
};

struct UM_PLAYWAIT {
    _UM_HEADER;
    int timeout;
};

#define PLAY_LOADING_TIMEOUT 10
struct UM_PLAYLOADING {
    _UM_HEADER;
    int8_t leasttime;  // least time of loading
    struct tmemberbrief member;
};

/////////////////////////////////////////////////////////////
// game login
struct UM_GAMELOGIN {
    _UM_HEADER;
    uint32_t charid;
    int roomid;
    uint32_t roomkey;
};

struct UM_GAMELOGINFAIL {
    _UM_HEADER;
    int8_t error;
};

struct UM_GAMELOGOUT {
    _UM_HEADER;
};

struct UM_GAMEINFO {
    _UM_HEADER;
    int8_t nmember;
    struct tmemberdetail members[0];
};
static inline uint16_t
UM_GAMEINFO_size(struct UM_GAMEINFO* um) {
    return sizeof(*um) + sizeof(um->members[0])*um->nmember;
}

struct UM_GAMEENTER {
    _UM_HEADER;
};

struct UM_GAMESTART {
    _UM_HEADER;
};

#pragma pack()

#endif
