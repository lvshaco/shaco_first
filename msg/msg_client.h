#ifndef __msg_client_h__
#define __msg_client_h__

#include "msg.h"
#include "msg_sharetype.h"

// IDUM
#define IDUM_CBEGIN 1000
#define IDUM_CEND   2000

#define IDUM_GATEB          IDUM_CBEGIN+0
#define IDUM_TEXT           IDUM_CBEGIN+0
// client -> server (route)
#define IDUM_GATEADDRREQ    IDUM_CBEGIN+1
// client -> server (bug)
#define IDUM_BUGSUBMIT      IDUM_CBEGIN+4

// client -> server (auth)
#define IDUM_LOGINACCOUNT   IDUM_CBEGIN+5

// client -> server (hall)
#define IDUM_HALLB          IDUM_CBEGIN+10  // hall begin
// user
#define IDUM_CHARCREATE     IDUM_CBEGIN+10

// role
#define IDUM_ROLEB          IDUM_CBEGIN+20
#define IDUM_USEROLE        IDUM_CBEGIN+20
#define IDUM_BUYROLE        IDUM_CBEGIN+21
#define IDUM_ADDROLE        IDUM_CBEGIN+22
#define IDUM_ADJUSTSTATE    IDUM_CBEGIN+23
#define IDUM_ROLEE          IDUM_CBEGIN+29

// ring
#define IDUM_RINGB          IDUM_CBEGIN+30
#define IDUM_RINGPAGEBUY    IDUM_CBEGIN+30
#define IDUM_RINGPAGERENAME IDUM_CBEGIN+31
#define IDUM_RINGEQUIP      IDUM_CBEGIN+32
#define IDUM_RINGSALE       IDUM_CBEGIN+33
#define IDUM_RINGPAGEUSE    IDUM_CBEGIN+34
#define IDUM_RINGE          IDUM_CBEGIN+39

// play
#define IDUM_PLAYB          IDUM_CBEGIN+40
#define IDUM_PLAY           IDUM_CBEGIN+41
#define IDUM_PLAYCANCEL     IDUM_CBEGIN+42
#define IDUM_PLAYE          IDUM_CBEGIN+46

// washgold
#define IDUM_WASHGOLDB      IDUM_CBEGIN+50
#define IDUM_WASHGOLD       IDUM_CBEGIN+51
#define IDUM_WASHGOLDE      IDUM_CBEGIN+59

#define IDUM_HALLE          IDUM_CBEGIN+499  // hall end

// client -> server (room)
#define IDUM_ROOMB          IDUM_CBEGIN+500  // room begin
#define IDUM_GAMESYNC       IDUM_CBEGIN+500
#define IDUM_PICKITEM       IDUM_CBEGIN+501
#define IDUM_ROLEPRESS      IDUM_CBEGIN+502
#define IDUM_GAMELOADOK     IDUM_CBEGIN+503
#define IDUM_USEITEM        IDUM_CBEGIN+504
#define IDUM_ROOME          IDUM_CBEGIN+599 // room end
#define IDUM_GATEE          IDUM_CBEGIN+599

// server -> client
// user
#define IDUM_LOGINACCOUNTFAIL IDUM_CBEGIN+600
#define IDUM_NOTIFYGATE     IDUM_CBEGIN+601
#define IDUM_NOTIFYWEB      IDUM_CBEGIN+602
#define IDUM_LOGINFAIL      IDUM_CBEGIN+603
#define IDUM_LOGOUT         IDUM_CBEGIN+604

// role
#define IDUM_CHARINFO       IDUM_CBEGIN+620
#define IDUM_SYNCMONEY      IDUM_CBEGIN+621
#define IDUM_SYNCSTATE      IDUM_CBEGIN+622
#define IDUM_ADJUSTSTATE_RES IDUM_CBEGIN+623
#define IDUM_SYNCEXP        IDUM_CBEGIN+624
#define IDUM_SYNCUSEROLE    IDUM_CBEGIN+625

// ring
#define IDUM_RINGPAGESYNC   IDUM_CBEGIN+630
#define IDUM_RINGSTACK      IDUM_CBEGIN+631

// play
#define IDUM_PLAYB2         IDUM_CBEGIN+640
#define IDUM_PLAYFAIL       IDUM_CBEGIN+641
#define IDUM_PLAYWAIT       IDUM_CBEGIN+642
#define IDUM_PLAYE2         IDUM_CBEGIN+649

// washgold
#define IDUM_WASHGOLD_INFO  IDUM_CBEGIN+650
#define IDUM_WASHGOLD_RES   IDUM_CBEGIN+651

// room
#define IDUM_GAMEINFO       IDUM_CBEGIN+800
#define IDUM_GAMEENTER      IDUM_CBEGIN+801
#define IDUM_GAMESTART      IDUM_CBEGIN+802
#define IDUM_GAMEUNJOIN     IDUM_CBEGIN+803
#define IDUM_ITEMEFFECT     IDUM_CBEGIN+804
#define IDUM_ROLEINFO       IDUM_CBEGIN+805
#define IDUM_GAMEOVER       IDUM_CBEGIN+806
#define IDUM_GAMEMEMBER     IDUM_CBEGIN+807
#define IDUM_ITEMUNEFFECT   IDUM_CBEGIN+808
#define IDUM_GAMEEXIT       IDUM_CBEGIN+809
#define IDUM_USEITEM_NOTIFY IDUM_CBEGIN+810
#define IDUM_ROLEOXYGEN     IDUM_CBEGIN+811
#define IDUM_ROLEREFRESH    IDUM_CBEGIN+812

// heartbeat
#define IDUM_HEARTBEAT      IDUM_CBEGIN+900

// route
#define IDUM_GATEADDR       IDUM_CBEGIN+901
#define IDUM_GATEADDRFAIL   IDUM_CBEGIN+902
// bug
#define IDUM_BUGSUBMITRES   IDUM_CBEGIN+904

#pragma pack(1)
////////////////////////////////////////////////////////////
struct UM_GATEADDRREQ {
    _UM_HEADER;
};

struct UM_GATEADDR {
    _UM_HEADER;
    char ip[40];
    uint16_t port;
};

struct UM_GATEADDRFAIL {
    _UM_HEADER;
};

struct UM_TEXT {
    _UM_HEADER;
    char str[0];
};

struct UM_BUGSUBMIT {
    _UM_HEADER;
    char str[0];
};

struct UM_BUGSUBMITRES {
    _UM_HEADER;
    int8_t err;
};

// login account
struct UM_LOGINACCOUNT {
    _UM_HEADER;
    char account[ACCOUNT_NAME_MAX+1];
    char passwd[ACCOUNT_PASSWD_MAX+1];
};

struct UM_LOGINACCOUNTFAIL {
    _UM_HEADER;
    int32_t err;
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
    char webaddr[IP_LEN];
};

////////////////////////////////////////////////////////////
// heartbeat
struct UM_HEARTBEAT {
    _UM_HEADER;
};

////////////////////////////////////////////////////////////
// login

struct UM_LOGOUT {
    _UM_HEADER;
    int8_t err; // see SERR_OK, if err == SERR_OK, then gate force close connection
};

struct UM_LOGINFAIL {
    _UM_HEADER;
    int8_t err; // see SERR_OK
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

struct UM_SYNCUSEROLE {
    _UM_HEADER;
    uint32_t roleid;
    int32_t oxygen;
    int32_t body;
    int32_t quick;
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

struct UM_SYNCEXP {
    _UM_HEADER;
    uint16_t level;
    uint32_t exp;
};

struct UM_ADJUSTSTATE { // C->S
    _UM_HEADER;
    uint32_t role_typeid; // 指定角色
};

struct UM_ADJUSTSTATE_RES { // S->C
    _UM_HEADER;
    uint32_t role_typeid; // 指定角色
    uint8_t state_value; // 
    uint8_t big_adjust; // 是否大调整（播放大特效)
};

struct UM_SYNCSTATE { // S->C
    _UM_HEADER;
    uint32_t role_typeid; // 指定角色
    uint8_t state_value; //
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

// washgold

struct UM_WASHGOLD { // C->S
    _UM_HEADER;
};

struct UM_WASHGOLD_RES { // S->C
    _UM_HEADER;
    uint8_t gain; // 淘金获取 (最大255)
    uint8_t extra_gain; // 金币收益加成
    uint32_t washgold; // 当前淘金库存
};

struct UM_WASHGOLD_INFO { // S->C
    _UM_HEADER;
    uint32_t washgold; // 当前淘金库存
};

//////////////////////////////////////////////////////////////
// play
struct UM_PLAY {
    _UM_HEADER;
    int8_t type; // see ROOM_TYPE*
};

struct UM_PLAYCANCEL {
    _UM_HEADER;
};

struct UM_PLAYFAIL {
    _UM_HEADER;
    int8_t err;
};

struct UM_PLAYWAIT {
    _UM_HEADER;
    int timeout;
};

/////////////////////////////////////////////////////////////
// game login

#define ROOMS_CREATE 0
#define ROOMS_ENTER  1
#define ROOMS_START  2
#define ROOMS_OVER   3

struct UM_GAMEINFO {
    _UM_HEADER;
    int8_t load_least_time;
    int8_t status;
    struct groundattri gattri;
    int8_t nmember;
    struct tmemberdetail members[0];
};
static inline uint16_t
UM_GAMEINFO_size(struct UM_GAMEINFO* um) {
    return sizeof(*um) + sizeof(um->members[0])*um->nmember;
}

struct UM_GAMEMEMBER {
    _UM_HEADER;
    struct tmemberdetail member;
};

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
    struct tmemberstat stat;
};

struct UM_GAMESYNC {
    _UM_HEADER;
    uint32_t charid;
    uint32_t depth;
};

struct UM_USEITEM_NOTIFY {
    _UM_HEADER;
    uint32_t itemid;
};

struct UM_USEITEM {
    _UM_HEADER;
    uint32_t itemid;
};

struct UM_PICKITEM {
    _UM_HEADER;
    uint32_t itemid;
};

#define ITEM_USE_T_PICK 1
#define ITEM_USE_T_SERVER 2
struct UM_ITEMEFFECT {
    _UM_HEADER;
    uint32_t spellid; // 释放者ID
    uint32_t oriitem; // 未知道具ID
    uint32_t itemid;  // 真实道具ID
    int8_t use_type; // ITEM_USE_T_; 
    uint8_t ntarget;
    uint32_t targets[];  // 目标ID
};
static inline uint16_t
UM_ITEMEFFECT_size(struct UM_ITEMEFFECT *um) {
    return sizeof(*um) + sizeof(um->targets[0])*um->ntarget;
}

struct UM_ITEMUNEFFECT {
    _UM_HEADER;
    uint32_t charid;  // 目标ID
    uint32_t itemid;  // 真实道具ID
};

struct UM_ROLEPRESS {
    _UM_HEADER;
    uint32_t charid;
};

struct UM_ROLEOXYGEN {
    _UM_HEADER;
    uint32_t oxygen;
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

struct UM_GAMEEXIT {
    _UM_HEADER;
    int8_t err;
};

struct UM_ROLEREFRESH {
    _UM_HEADER;
    uint32_t charid;
    uint8_t flags[FLAG_MAX];
    uint32_t data[];
};

#pragma pack()

#endif
