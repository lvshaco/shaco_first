#ifndef __player_h__
#define __player_h__

#include "sharetype.h"
#include <stdint.h>

#define PS_FREE  0
#define PS_QUERYCHAR 1
#define PS_WAITCREATECHAR 2
#define PS_CHECKCHARNAME 3
#define PS_CHARUNIQUEID 4
#define PS_CREATECHAR 5
#define PS_LOADCHAR 6
#define PS_LOGIN 1
#define PS_GAME  2
#define PS_WAITING  3
#define PS_CREATING 4
#define PS_ROOM 5

struct player {
    uint16_t gid;
    uint16_t cid;
    int status;
    int createchar_times;
    int roomid;
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
