#include "sc_service.h"
#include "sharetype.h"
#include "player.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdlib.h>
#include <string.h>

static void
_effect(struct char_attribute* cattri, const struct role_tplt* base, 
        int32_t type, int32_t value, bool isper) {
#define CASE(T, R, B, V, isper) \
    case T: (R) += (isper) ? (B)*(V)*0.001 : (V); break;

    switch (type) {
    CASE(EFFECT_OXYGEN, cattri->oxygen, base->oxygen, value, isper);
    CASE(EFFECT_BODY, cattri->body, base->body, value, isper);
    CASE(EFFECT_QUICK, cattri->quick, base->quick, value, isper);
    CASE(EFFECT_COIN_PROFIT, cattri->coin_profit, 1, value, 1);
    CASE(EFFECT_MOVE_SPEED, cattri->movespeedadd, 1, value, 1);
    CASE(EFFECT_FALL_SPEED, cattri->charfallspeedadd, 1, value, 1);
    CASE(EFFECT_ATTACK_DISTANCE, cattri->attack_distance, base->attack_distance, value, isper);
    CASE(EFFECT_ATTACK_RANGE, cattri->attack_range, base->attack_range, value, isper);
    CASE(EFFECT_ATTACK_POWER, cattri->attack_power, base->attack_power, value, isper);
    CASE(EFFECT_LUCK, cattri->lucky, base->lucky, value, isper);
    CASE(EFFECT_ATTACK_SPEED, cattri->attack_speed, base->attack_speed, value, isper);
    CASE(EFFECT_DODGE_DISTANCE, cattri->dodgedistanceadd, 1, value, 1);  
    CASE(EFFECT_REBIRTH_TIME, cattri->rebirthtimeadd, 1, value, 1);
    CASE(EFFECT_JUMP_RANGE, cattri->jump_range, base->jump_range, value, isper); 
    CASE(EFFECT_SENCE_RANGE, cattri->sence_range, base->sence_range, value, isper);
    CASE(EFFECT_WINCOIN_PROFIT, cattri->wincoin_profit, 1, value, 1);
    CASE(EFFECT_EXP_PROFIT, cattri->exp_profit, 1, value, 1);
    CASE(EFFECT_ITEM_TIME, cattri->item_timeadd, 1, value, 1);
    CASE(EFFECT_ITEM_OXYGEN, cattri->item_oxygenadd, 1, value, 1);
    //todo
    //CASE(EFFECT_PRICES, cattri->prices, 1, value, 1);
    //CASE(EFFECT_SKILL_ACTIVE);
    //CASE(EFFECT_SKILL_PASSIVE);
    CASE(EFFECT_VIEW_RANGE, cattri->view_range, base->view_range, value, isper);
    CASE(EFFECT_SCORE_PROFIT, cattri->score_profit, 1, value, 1);
    CASE(EFFECT_WINSCORE_PROFIT, cattri->winscore_profit, 1, value, 1);
    }
} 

#define EFFECT(cattri, base, type, value, isper) \
    if (type > 0 && value > 0) { \
        _effect(cattri, base, type, value, isper); \
    }

void
attribute_service(struct service* s, struct service_message* sm) {
    struct player* p = sm->msg;
    struct chardata* cdata = &p->data; 
    struct ringdata* rdata = &cdata->ringdata;
    struct char_attribute* cattri = &cdata->attri;

    const struct role_tplt* base = tplt_find(TPLT_ROLE, cdata->role);
    if (base == NULL)
        return;

    memset(cattri, 0, sizeof(*cattri)); 
    EFFECT(cattri, base, base->effect1, base->value1, base->valuet1);
    EFFECT(cattri, base, base->effect2, base->value2, base->valuet2);
    EFFECT(cattri, base, base->effect3, base->value3, base->valuet3);
    EFFECT(cattri, base, base->effect4, base->value4, base->valuet4);
    EFFECT(cattri, base, base->effect5, base->value5, base->valuet5);
    
    if (rdata->usepage >= rdata->npage) {
        rdata->usepage = 0;
    }
    const struct ring_tplt* ring;
    uint32_t ringid;
    struct ringpage* page = &rdata->pages[rdata->usepage];
    int i;
    for (i=0; i<RING_PAGE_SLOT; ++i) {
        ringid = page->slots[i];
        if (ringid > 0) {
            ring = tplt_find(TPLT_RING, ringid);
            if (ring) {
                EFFECT(cattri, base, base->effect1, base->value1, base->valuet1);
            }
        }
    }
    cattri->oxygen += base->oxygen;
    cattri->body += base->body;
    cattri->quick += base->quick;
    cattri->lucky += base->lucky;
    cattri->attack_distance += base->attack_distance;
    cattri->attack_range += base->attack_range;
    cattri->attack_speed += base->attack_speed;
    cattri->attack_power += base->attack_power;
    cattri->jump_range += base->jump_range;
    cattri->sence_range += base->sence_range;
    cattri->view_range += base->view_range;
    cattri->coin_profit += base->coin_profit*0.01f;
}
