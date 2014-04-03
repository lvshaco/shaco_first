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
// Award score, exp
// Rank score
// Rank reset

#define ACCID_BEGIN   1001
#define ACCID_END     900000
#define CHARID_BEGIN  1001
#define CHARID_END    900000
/*#define ACCID_BEGIN  2000000*/
//#define ACCID_END    3000000
//#define CHARID_BEGIN 2000000
/*#define CHARID_END   3000000*/
#define ROBOT_MAX       min((CHARID_END-CHARID_BEGIN+1), (ACCID_END-ACCID_BEGIN+1))

#define S_REST   0
#define S_WAIT   1
#define S_FIGHT  2

#define AI_MAX   10
#define UID(ag) ((ag)->data.accid)

struct tplt;

struct agent { 
    int status;
    int ai;
    struct chardata data;
    uint32_t last_change_role_time;
    struct agent *next;
};

struct agent_list {
    struct agent *head; 
    struct agent *tail;
};

struct robot {
    struct tplt *T;
    int match_handle;
    int room_handle;
    struct sh_hash agents;
    struct agent_list rests[AI_MAX];
};

#endif
