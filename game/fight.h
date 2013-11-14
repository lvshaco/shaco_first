#ifndef __fight_h__
#define __fight_h__

#include "sharetype.h"
#include <stdint.h>

static inline void 
ground_attri_build(int32_t difficulty, struct groundattri* ga) {
    difficulty = difficulty*0.01;
    if (difficulty < 1.0)
        difficulty = 1.0;
    else if (difficulty > 10.0)
        difficulty = 10.0;

    float factor = (1+(difficulty-5)/10.0);
    factor *= factor;
    factor += 0.5;
    ga->difficulty = difficulty;
    ga->shaketime = 1800 * (1- (difficulty-1)/20.0);
    ga->cellfallspeed = 4.167 + (difficulty-1) * 0.833;
    ga->waitdestroy = ga->shaketime * 2/3.0;
    ga->destroytime = 400 * (1 - (difficulty-1)/15.0);
}

static inline float
role_body_factor(struct tmemberdetail* cdata) {
    return (1+(5-cdata->bodycur*0.01)/20.0);
}

static inline void 
role_attri_build(const struct groundattri* ga,
                 struct tmemberdetail* cdata) {
    float quick = cdata->quickcur*0.01;
   
    float bodyfactor = role_body_factor(cdata);
    float factor = 1 - (5-quick)/((quick > 5) ? 10.0 : 20.0);
    
    cdata->movespeed = ga->cellfallspeed*2/3.0 * factor;
    cdata->charfallspeed = ga->cellfallspeed * ((quick>5) ? factor : 1);
    cdata->jmpspeed = 3 * cdata->movespeed * factor;
    cdata->jmpacctime = 0.25 * ga->shaketime * (2-factor);
    cdata->rebirthtime = 2200 * (1-(ga->difficulty-1)/20.0) * (2-factor)*(2-bodyfactor);
    cdata->dodgedistance = (factor * factor * 5 + 10)/60.0;
}

static inline float
role_oxygen_time_consume(struct tmemberdetail* cdata) {
    float bodyfactor = role_body_factor(cdata);
    return 10* (2-bodyfactor);
}

#endif
