#ifndef __room_fight_h__
#define __room_fight_h__

#include "msg_sharetype.h"
#include <stdint.h>
#include <math.h>

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
    ga->shaketime = 1600 * (1- (difficulty-1)/16.0);
    ga->cellfallspeed = 4.167 + (difficulty-1) * 0.833;
    ga->waitdestroy = ga->shaketime * 2/3.0;
    ga->destroytime = 400 * (1 - (difficulty-1)/15.0);
}

static inline float
role_body_factor(struct char_attribute* cattri) {
    return 1-(5-cattri->body*0.01)/10.0;
}

static inline void 
role_attri_build(const struct groundattri* ga,
                 struct char_attribute* cattri) {
    float quick = cattri->quick*0.01;
   
    float bodyfactor = role_body_factor(cattri);
    float factor = 1 - (5-quick)/((quick > 5) ? 10.0 : 20.0);
    
    cattri->movespeed = ga->cellfallspeed*2/3.0 * factor;
    cattri->movespeed += cattri->movespeed * cattri->movespeedadd;

    cattri->charfallspeed = ga->cellfallspeed * ((quick>5) ? factor : 1);
    cattri->charfallspeed += cattri->charfallspeed * cattri->charfallspeedadd;

    cattri->jmpspeed = 2 * cattri->movespeed * factor;
    cattri->jmpacctime = 0.18 * ga->shaketime * (2-factor);

    cattri->rebirthtime = 2500 * (2-bodyfactor);
    cattri->rebirthtime += cattri->rebirthtime * cattri->rebirthtimeadd;

    cattri->dodgedistance = (factor * factor * 5 + 10)/60.0;
    cattri->dodgedistance += cattri->dodgedistance * cattri->dodgedistanceadd;
}

static inline float
role_oxygen_time_consume(struct groundattri *ga, struct char_attribute* cattri) {
    float bodyfactor = role_body_factor(cattri);
    return (1.05 * ga->cellfallspeed + 2) * pow(2-bodyfactor, 0.4);
}

#endif
