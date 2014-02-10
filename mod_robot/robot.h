#ifndef __robot_h__
#define __robot_h__

#include "msg_sharetype.h"
#include "sh_hash.h"

// Accid  reserve 1001~1000000, create from 1000001
// Charid reserve 1001~1000000, create from 1000001
// Create robot, insert to db
// Load  robot to memory
// Apply robot to match
// Login robot to room
// Award shore, exp
// Rank shore
// Rank reset

//#define ACCID_BEGIN 1001
//#define ACCID_END 1000000
//#define CHARID_BEGIN 1001
//#define CHARID_END 1000000
#define ACCID_BEGIN  2000000
#define ACCID_END    3000000
#define CHARID_BEGIN 2000000
#define CHARID_END   3000000
#define ROBOT_MAX       min((CHARID_END-CHARID_BEGIN+1), (ACCID_END-ACCID_BEGIN+1))

#define S_REST   0
#define S_WAIT   1
#define S_FIGHT  2

#define UID(ag) ((ag)->data.accid)

struct agent { 
    int status;
    int level;
    struct chardata data;
    uint32_t last_change_role_time;
    struct agent *next;
};

struct tplt;

struct robot {
    struct tplt *T;
    int match_handle;
    int room_handle;
    int nagent;
    struct agent *rest_head; 
    struct agent *rest_tail;
    struct sh_hash agents;
};

#endif
