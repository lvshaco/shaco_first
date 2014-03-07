#include "room.h"
#include "sh_log.h"

void 
room_dump_ground(struct room_game *ro) {
    const struct groundattri *ga = &ro->gattri;

    sh_rec("mapid: %u", ga->mapid);
    sh_rec("difficulty: %d", ga->difficulty);
    sh_rec("shaketime: %d", ga->shaketime);
    sh_rec("cellfallspeed: %f", ga->cellfallspeed);
    sh_rec("waitdestroy: %d", ga->waitdestroy);
    sh_rec("destroytime: %d", ga->destroytime);
}

void 
room_dump_player(struct player *m) {
    struct tmemberdetail  *d = &m->detail; 
    struct char_attribute *a = &d->attri;
    
    sh_rec("accid: id %u, name %s", d->accid, d->name);
    sh_rec("oxygen: %d", a->oxygen);     // 氧气
    sh_rec("body: %d", a->body);       // 体能
    sh_rec("quick: %d", a->quick);      // 敏捷

    sh_rec("movespeed: %f", a->movespeed);     // 移动速度
    sh_rec("movespeedadd: %f", a->movespeedadd);
    sh_rec("charfallspeed: %f", a->charfallspeed); // 坠落速度
    sh_rec("charfallspeedadd: %f", a->charfallspeedadd);
    sh_rec("jmpspeed: %f", a->jmpspeed);      // 跳跃速度--
    sh_rec("jmpacctime: %d", a->jmpacctime);  // 跳跃准备时间--
    sh_rec("rebirthtime: %d", a->rebirthtime); // 复活时间
    sh_rec("rebirthtimeadd: %f", a->rebirthtimeadd);
    sh_rec("dodgedistance: %f", a->dodgedistance); // 闪避距离
    sh_rec("dodgedistanceadd: %f", a->dodgedistanceadd);
    sh_rec("jump_range: %d", a->jump_range);  // 跳跃高度
    sh_rec("sence_range: %d", a->sence_range); // 感知范围
    sh_rec("view_range: %d", a->view_range);  // 视野范围
   
    sh_rec("attack_power: %d", a->attack_power);
    sh_rec("attack_distance: %d", a->attack_distance);
    sh_rec("attack_range: %d", a->attack_range);
    sh_rec("attack_speed: %d", a->attack_speed);

    sh_rec("coin_profit: %f", a->coin_profit);
    sh_rec("wincoin_profit: %f", a->wincoin_profit);
    sh_rec("score_profit: %f", a->score_profit);
    sh_rec("winscore_profit: %f", a->winscore_profit);
    sh_rec("exp_profit: %f", a->exp_profit);
    sh_rec("item_timeadd: %f", a->item_timeadd);
    sh_rec("item_oxygenadd: %f", a->item_oxygenadd);
    sh_rec("lucky: %d", a->lucky);
    sh_rec("prices: %d", a->prices);
}
