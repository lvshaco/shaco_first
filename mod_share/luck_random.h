#ifndef __luck_random_h__
#define __luck_random_h__

#include <math.h>
#include "sh_util.h"

static inline int
luck_random(uint32_t randseed, int32_t lucky, float radiate, int uprand, float* luck_factor) {
    if (uprand <= 0) {
        return 0;
    }
    float luck_base = lucky / 100.0;
    
    int section_down  = max((*luck_factor - radiate) * uprand, 0);
    int section_up    = min((*luck_factor + radiate) * uprand, uprand);
    int randx = sh_rand(randseed) % (section_up - section_down + 1) + section_down;

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
    *luck_factor += luck_factor_wave;
    if (*luck_factor > 1)
        *luck_factor = 1;
    else if (*luck_factor < 0)
        *luck_factor = 0;
    return randx;
}

#endif
