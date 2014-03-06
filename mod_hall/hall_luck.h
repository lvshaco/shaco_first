#ifndef __hall_luck_h__
#define __hall_luck_h__

#include "luck_random.h"
#include "hall.h"
#include "hall_player.h"

static inline int
hall_luck_random(struct hall *self, struct player *pr, float radiate, int uprand) {
    if (uprand <= 0) {
        return 0;
    }
    struct chardata *cdata = &pr->data;
    return luck_random(self->randseed, cdata->attri.lucky, radiate, uprand, &cdata->luck_factor);
}

static inline float
hall_luck_random_float(struct hall *self, struct player *pr, float radiate, int uprand) {
    if (uprand > 0) {
        return hall_luck_random(self, pr, radiate, uprand) / (float)uprand;
    }
    return 0.0f;
}

#endif
