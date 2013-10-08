#ifndef __player_h__
#define __player_h__

#include "sharetype.h"
#include <stdint.h>

#define PS_FREE  0
#define PS_LOGIN 1
#define PS_GAME  2 
struct player {
    uint16_t gid;
    uint16_t cid;
    int status;
    struct chardata data;
};
void _allocplayers(int cmax, int hmax, int gmax);
void _freeplayers();
struct player* _getplayer(uint16_t gid, int cid);
struct player* _getplayerbyid(uint32_t charid);
struct player* _allocplayer(uint16_t gid, int cid);
void _freeplayer(struct player* p);
int _hashplayer(struct player* p);

#endif
