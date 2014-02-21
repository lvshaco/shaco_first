#ifndef __room_h__
#define __room_h__

#include "sh_hash.h"
#include <stdint.h>

struct tplt;

struct room {
    struct tplt *T;
    struct sh_hash *MH; 
    //int watchdog_handle;
    //int match_handle;
    int tick;
    uint32_t randseed;
    uint32_t map_randseed;
    struct sh_hash players;
    struct sh_hash gamerooms;
};

#endif
