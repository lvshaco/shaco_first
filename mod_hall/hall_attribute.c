#include "sc.h"
#include "hall.h"
#include "hall_tplt.h"
#include "hall_player.h"
#include "msg_server.h"

static void
effect(struct char_attribute* cattri, const struct role_tplt* base, 
        int32_t type, int32_t value, bool isper) {
#define CASE(T, R, B, V, isper) \
    case T: (R) += (isper) ? (B)*(V)/1000.f : (V); break;

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
        effect(cattri, base, type, value, isper); \
    }

/*
static void dump(struct chardata* cdata) {
    sh_rec("char: accid%u, id %u, name %s", cdata->accid, cdata->charid, cdata->name);
    struct char_attribute* attri = &cdata->attri;
    sh_rec("role: %d", cdata->role);
    sh_rec("oxygen: %d", attri->oxygen);     // 氧气
    sh_rec("body: %d", attri->body);       // 体能
    sh_rec("quick: %d", attri->quick);      // 敏捷
    
    sh_rec("movespeed: %f", attri->movespeed);     // 移动速度
    sh_rec("movespeedadd: %f", attri->movespeedadd);
    sh_rec("charfallspeed: %f", attri->charfallspeed); // 坠落速度
    sh_rec("charfallspeedadd: %f", attri->charfallspeedadd);
    sh_rec("jmpspeed: %f", attri->jmpspeed);      // 跳跃速度--
    sh_rec("jmpacctime: %d", attri->jmpacctime);  // 跳跃准备时间--
    sh_rec("rebirthtime: %d", attri->rebirthtime); // 复活时间
    sh_rec("rebirthtimeadd: %f", attri->rebirthtimeadd);
    sh_rec("dodgedistance: %f", attri->dodgedistance); // 闪避距离
    sh_rec("dodgedistanceadd: %f", attri->dodgedistanceadd);
    sh_rec("jump_range: %d", attri->jump_range);  // 跳跃高度
    sh_rec("sence_range: %d", attri->sence_range); // 感知范围
    sh_rec("view_range: %d", attri->view_range);  // 视野范围
   
    sh_rec("attack_power: %d", attri->attack_power);
    sh_rec("attack_distance: %d", attri->attack_distance);
    sh_rec("attack_range: %d", attri->attack_range);
    sh_rec("attack_speed: %d", attri->attack_speed);

    sh_rec("coin_profit: %f", attri->coin_profit);
    sh_rec("wincoin_profit: %f", attri->wincoin_profit);
    sh_rec("score_profit: %f", attri->score_profit);
    sh_rec("winscore_profit: %f", attri->winscore_profit);
    sh_rec("exp_profit: %f", attri->exp_profit);
    sh_rec("item_timeadd: %f", attri->item_timeadd);
    sh_rec("item_oxygenadd: %f", attri->item_oxygenadd);
    sh_rec("lucky: %d", attri->lucky);
    sh_rec("prices: %d", attri->prices);
}
*/
void
attribute_refresh(struct tplt *T, struct chardata *cdata) {
    struct ringdata* rdata = &cdata->ringdata;
    struct char_attribute* cattri = &cdata->attri;

    const struct role_tplt* base = tplt_find(T, TPLT_ROLE, cdata->role);
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
            ring = tplt_find(T, TPLT_RING, ringid);
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
    cattri->coin_profit += base->coin_profit/100.f;

    //dump(cdata);
}

void
hall_attribute_main(struct tplt *T, struct chardata *cdata) {
    attribute_refresh(T, cdata);
}
