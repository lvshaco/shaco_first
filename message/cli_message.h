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
#define IDUM_CHARCREATE     IDUM_CBEGIN+5
#define IDUM_LOGINACCOUNT   IDUM_CBEGIN+10
#define IDUM_LOGINACCOUNTFAIL IDUM_CBEGIN+11
#define IDUM_NOTIFYGATE IDUM_CBEGIN+12
#define IDUM_NOTIFYWEB      IDUM_CBEGIN+13

// role
#define IDUM_USEROLE        IDUM_CBEGIN+20
#define IDUM_BUYROLE        IDUM_CBEGIN+21
#define IDUM_ADDROLE        IDUM_CBEGIN+22
#define IDUM_SYNCMONEY      IDUM_CBEGIN+23

// ring
#define IDUM_RINGPAGEBUY    IDUM_CBEGIN+30
#define IDUM_RINGPAGERENAME   IDUM_CBEGIN+31
#define IDUM_RINGEQUIP      IDUM_CBEGIN+32
#define IDUM_RINGSALE       IDUM_CBEGIN+33
#define IDUM_RINGPAGESYNC   IDUM_CBEGIN+34
#define IDUM_RINGSTACK      IDUM_CBEGIN+35
#define IDUM_RINGPAGEUSE    IDUM_CBEGIN+36

// play
#define IDUM_PLAY           IDUM_CBEGIN+100
#define IDUM_PLAYFAIL       IDUM_CBEGIN+101
#define IDUM_PLAYWAIT       IDUM_CBEGIN+102
#define IDUM_PLAYLOADING    IDUM_CBEGIN+103
#define IDUM_PLAYBEGIN      IDUM_CBEGIN+104
#define IDUM_PLAYJOIN       IDUM_CBEGIN+105
#define IDUM_PLAYUNJOIN     IDUM_CBEGIN+106
#define IDUM_PLAYDONE       IDUM_CBEGIN+107

// game
#define IDUM_NOTIFYGAME     IDUM_CBEGIN+200
#define IDUM_GAMELOGIN      IDUM_CBEGIN+201
#define IDUM_GAMELOGINFAIL  IDUM_CBEGIN+202
#define IDUM_GAMELOGOUT     IDUM_CBEGIN+203
#define IDUM_GAMEINFO       IDUM_CBEGIN+204
#define IDUM_GAMEENTER      IDUM_CBEGIN+205
#define IDUM_GAMESTART      IDUM_CBEGIN+206
#define IDUM_GAMEUNJOIN     IDUM_CBEGIN+207
#define IDUM_GAMESYNC       IDUM_CBEGIN+208
#define IDUM_USEITEM        IDUM_CBEGIN+209
#define IDUM_ITEMEFFECT     IDUM_CBEGIN+210
#define IDUM_ROLEPRESS      IDUM_CBEGIN+211
#define IDUM_ROLEINFO       IDUM_CBEGIN+212
#define IDUM_GAMEOVER       IDUM_CBEGIN+213
#define IDUM_GAMELOADOK     IDUM_CBEGIN+214

#pragma pack(1)
////////////////////////////////////////////////////////////
// login account
struct UM_LOGINACCOUNT {
    _UM_HEADER;
    char account[ACCOUNT_NAME_MAX];
    char passwd[ACCOUNT_PASSWD_MAX];
};

struct UM_LOGINACCOUNTFAIL {
    _UM_HEADER;
    int32_t error;
};

struct UM_NOTIFYGATE {
    _UM_HEADER;
    uint32_t accid;
    uint64_t key;
    uint32_t addr;
    uint16_t port;
};

struct UM_NOTIFYWEB {
    _UM_HEADER;
    uint32_t webaddr;
};

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
    int32_t  roomid;
};

////////////////////////////////////////////////////////////
// login
struct UM_LOGIN {
    _UM_HEADER;
    uint32_t accid;
    uint64_t key;
    char account[ACCOUNT_NAME_MAX];
};

struct UM_LOGOUT {
    _UM_HEADER;
    int32_t error; // see SERR_OK
};

struct UM_LOGINFAIL {
    _UM_HEADER;
    int32_t error; // see SERR_OK
};

struct UM_CHARCREATE {
    _UM_HEADER;
    char name[CHAR_NAME_MAX];
};

struct UM_CHARINFO {
    _UM_HEADER;
    struct chardata data;
};

// role
struct UM_USEROLE {
    _UM_HEADER;
    uint32_t roleid;
};

struct UM_BUYROLE {
    _UM_HEADER;
    uint32_t roleid;
};

struct UM_ADDROLE {
    _UM_HEADER;
    uint32_t roleid;
};

struct UM_SYNCMONEY {
    _UM_HEADER;
    uint32_t coin;
    uint32_t diamond;
};

// ring
struct UM_RINGPAGEUSE { // C -> S
    _UM_HEADER;
    uint8_t index;
};

struct UM_RINGPAGEBUY { // C -> S
    _UM_HEADER;
};

struct UM_RINGPAGESYNC { // S -> C
    _UM_HEADER;
    uint8_t curpage; // 当前拥有的页数
};

struct UM_RINGPAGERENAME { // C -> S
    _UM_HEADER;
    uint8_t index;
    char name[RING_PAGE_NAME];
};

struct UM_RINGEQUIP { // C -> S 
    _UM_HEADER;
    uint8_t index;
    uint32_t rings[RING_PAGE_SLOT]; // 整个记录页装备的戒指
};

struct UM_RINGSALE { // C -> S 
    _UM_HEADER;
    uint32_t ringid;
    uint8_t  count;
};

struct UM_RINGSTACK { // S -> C 
    _UM_HEADER;
    uint32_t ringid;
    uint8_t stack; // 当前堆叠
};

//////////////////////////////////////////////////////////////
// play
struct UM_PLAY {
    _UM_HEADER;
    int8_t type; // see ROOM_TYPE*
};

struct UM_PLAYFAIL {
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
    struct groundattri gattri;
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

struct UM_GAMELOADOK {
    _UM_HEADER;
};

struct UM_GAMEUNJOIN {
    _UM_HEADER;
    uint32_t charid;
};

struct UM_GAMESYNC {
    _UM_HEADER;
    uint32_t charid;
    uint32_t depth;
};

struct UM_USEITEM {
    _UM_HEADER;
    uint32_t itemid;
};

struct UM_ITEMEFFECT {
    _UM_HEADER;
    uint32_t spellid;
    uint32_t oriitem;
    uint32_t charid;
    uint32_t itemid;
};

struct UM_ROLEPRESS {
    _UM_HEADER;
    uint32_t charid;
};

struct UM_ROLEINFO {
    _UM_HEADER;
    struct tmemberdetail detail;
};

struct UM_GAMEOVER {
    _UM_HEADER;
    uint8_t type; // ROOM_TYPE*
    int8_t nmember;
    struct tmemberstat stats[0];
};
static inline uint16_t
UM_GAMEOVER_size(struct UM_GAMEOVER* um) {
    return sizeof(*um) + sizeof(um->stats[0])*um->nmember;
}

#pragma pack()

#endif
