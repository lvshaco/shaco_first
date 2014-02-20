#ifndef __hall_luck_h__
#define __hall_luck_h__

#include <math.h>
#include "sh_util.h"
#include "hall.h"
#include "hall_player.h"

static inline int
hall_luck_random(struct hall* self, struct player *pr, float radiate, int uprand) {
    if (uprand <= 0) {
        return 0;
    }
    struct chardata *cdata = &pr->data;
    
    float luck_base = cdata->attri.lucky / 100.0;
    float luck_factor = cdata->luck_factor;
    
    int section_down  = max((luck_factor - radiate) * uprand, 0);
    int section_up    = min((luck_factor + radiate) * uprand, uprand);
    int randx = sh_rand(self->randseed) % (section_up - section_down + 1) + section_down;

    float factor = 3.162277660168379;
    float rare = ((randx * 10 / uprand) / factor);
    rare *= rare;

    float luck_factor_wave;
    float rare_diff = luck_base - rare;
    if (rare_diff <= 0) {
        luck_factor_wave = -(rare_diff * rare_diff);
    } else {
        luck_factor_wave = pow(rare_diff, 0.5);
    }
    luck_factor_wave *= 0.1;
    cdata->luck_factor += luck_factor_wave;
    if (cdata->luck_factor > 1)
        cdata->luck_factor = 1;
    else if (cdata->luck_factor < 0)
        cdata->luck_factor = 0;
    return randx;
}

#endif
