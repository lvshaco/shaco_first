#ifndef __player_h__
#define __player_h__

#include "sharetype.h"
#include <stdint.h>

#define PS_FREE  0
#define PS_QUERYCHAR 1
#define PS_WAITCREATECHAR 2
#define PS_CHECKCHARNAME 3
#define PS_SAVECHARNAME 4
#define PS_CHARUNIQUEID 5
#define PS_CREATECHAR 6
#define PS_BINDCHARID 7
#define PS_LOADCHAR 8
#define PS_LOGIN 9
#define PS_GAME  10
#define PS_WAITING  11
#define PS_CREATING 12
#define PS_ROOM 13

// level grade
#define LV_RUMEN   0
#define LV_XINSHOU 1
#define LV_CHUJI   2
#define LV_ZHONGJI 3
#define LV_GAOJI   4
#define LV_DAREN   5
#define LV_MAX     6

static inline const char*
_player_gradestr(int grade) {
    static const char* _STR[LV_MAX] = {
        "",
        "xinshou",
        "chuji",
        "zhongji",
        "gaoji",
        "daren",
    };
    if (grade >= LV_RUMEN && grade < LV_MAX)
        return _STR[grade];
    return "";
}

static inline int
_player_gradeid(uint16_t level) {
    int grade = (level + 8) / 9;
    if (grade >= LV_MAX)
        grade = LV_MAX - 1;
    return grade;
}

// player
struct player {
    uint16_t gid;
    uint16_t cid;
    int status;
    int createchar_times;
    int roomid;
    int cu_flag; // see CU_GRADE
    struct chardata data;
};

void _allocplayers(int cmax, int hmax, int gmax);
void _freeplayers();
struct player* _getplayer(uint16_t gid, int cid);
struct player* _getplayerbycharid(uint32_t charid);
struct player* _getplayerbyaccid(uint32_t accid);
struct player* _allocplayer(uint16_t gid, int cid);
void _freeplayer(struct player* p);
int  _hashplayeracc(struct player* p, uint32_t accid);
int  _hashplayer(struct player* p, uint32_t charid);

#endif
