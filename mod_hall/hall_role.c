#include "hall.h"
#include "hall_role.h"
#include "hall_tplt.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "hall_attribute.h"
#include "hall_luck.h"
#include "msg_server.h"
#include "msg_client.h"
#include "sh.h"

#define ROLE_DEF 10 // 默认给予ID

static bool
has_role(struct chardata* cdata, uint32_t roleid) {
    uint32_t typeid = ROLE_TYPEID(roleid);
    uint32_t clothid = ROLE_CLOTHID(roleid);
    if (IS_VALID_TYPEID(typeid)) {
        if (IS_VALID_CLOTHID(clothid)) {
            return cdata->ownrole[typeid] & (1<<clothid);
        }
    }
    return false;
}

static inline bool
has_anyrole(struct chardata *cdata, uint32_t typeid) {
    if (IS_VALID_TYPEID(typeid)) {
        return cdata->ownrole[typeid] > 0;
    }
    return false;
}

static int
add_role(struct chardata* cdata, uint32_t roleid) {
    uint32_t typeid = ROLE_TYPEID(roleid);
    uint32_t clothid = ROLE_CLOTHID(roleid);
    if (typeid >= 0 && typeid < ROLE_MAX) {
        if (clothid >= 0 && clothid < ROLE_CLOTHES_MAX) {
            cdata->ownrole[typeid] |= 1<<clothid;
            cdata->roles_state[typeid] = STATE_INIT_VALUE;
            return 0;
        }
    }
    return 0;
}

static inline void
sync_addrole(struct module *s, struct player* pr, uint32_t roleid) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_ADDROLE, ar);
    cl->uid  = UID(pr);
    ar->roleid = roleid;
    sh_handle_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*ar));
}

static inline void
sync_userole(struct module *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_SYNCUSEROLE, sync);
    cl->uid  = UID(pr);
    sync->roleid = pr->data.role;
    sync->oxygen = pr->data.attri.oxygen;
    sync->body = pr->data.attri.body;
    sync->quick = pr->data.attri.quick;
    sh_handle_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*sync));
}

static inline void
notify_adjust_result(struct module *s, struct player *pr, uint32_t typeid, uint8_t state_value, 
        uint8_t big_adjust) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_ADJUSTSTATE_RES, res);
    cl->uid  = UID(pr);
    res->role_typeid = typeid;
    res->state_value = state_value;
    res->big_adjust = big_adjust;
    sh_handle_send(MODULE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*res));
}

static inline void
use_role(struct chardata* cdata, const struct role_tplt* tplt) {
    cdata->role = tplt->id;
}

static void
process_userole(struct module *s, struct player *pr, const struct UM_USEROLE *use) {
    struct hall *self = MODULE_SELF;

    struct chardata* cdata = &pr->data;
    uint32_t roleid  = use->roleid;

    if (roleid == cdata->role) {
        return;
    }
    const struct role_tplt* tplt = tplt_find(self->T, TPLT_ROLE, roleid);
    if (tplt == NULL) {
        return;
    }
    if (!has_role(cdata, roleid)) {
        return;
    }
    // do logic
    use_role(cdata, tplt);
  
    // refresh attribute
    hall_attribute_main(self->T, &pr->data);

    sync_userole(s, pr);
    
    hall_playerdb_save(s, pr, false);
    hall_gamelog(s, self->charactionlog_handle, "USEROLE,%u,%u,%u", 
            sh_timer_now()/1000, cdata->accid, roleid);
}

static void
process_buyrole(struct module *s, struct player *pr, const struct UM_BUYROLE *buy) {
    struct hall *self = MODULE_SELF;
    struct chardata* cdata = &pr->data;
    uint32_t roleid  = buy->roleid;
   
    if (has_role(cdata, roleid)) {
        return; // 已经拥有
    }
    const struct role_tplt* tplt = tplt_find(self->T, TPLT_ROLE, roleid);
    if (tplt == NULL) {
        return;
    }
    if (cdata->level < tplt->needlevel) {
        return; // 等级不足
    }
    if (cdata->coin < tplt->needcoin) {
        return; // 金币不足
    }
    if (cdata->diamond < tplt->needdiamond) {
        return; // 钻石不足
    }
    // do logic
    if (add_role(cdata, roleid)) {
        return;
    }
    uint32_t coin_old = cdata->coin;
    cdata->coin -= tplt->needcoin;
    cdata->diamond -= tplt->needdiamond;
    sync_addrole(s, pr, roleid);
    hall_sync_money(s, pr);

    hall_playerdb_save(s, pr, true);
    hall_gamelog(s, self->charactionlog_handle, "BUYROLE,%u,%u,%u,%u,%u", 
            sh_timer_now()/1000, cdata->accid, roleid, coin_old, cdata->coin);
}

static void
process_adjust_state(struct module *s, struct player *pr, const struct UM_ADJUSTSTATE *adjust) {
    struct hall *self = MODULE_SELF;
    struct chardata* cdata = &pr->data;
    uint32_t typeid  = adjust->role_typeid;

    if (!has_anyrole(cdata, typeid)) {
        return; // 没有角色
    }
    int old_value = cdata->roles_state[typeid];
    int old_id = role_state_id(old_value);
    if (old_id >= ROLE_STATE_4) {
        return;
    }
    static const int PAY[] = {10, 20, 50, 100};
    if (old_id < 0)
        old_id = 0;
    if (old_id >= sizeof(PAY)/sizeof(PAY[0]))
        old_id = sizeof(PAY)/sizeof(PAY[0])-1;

    int pay_coin  = PAY[old_id];
    if (cdata->coin < pay_coin) {
        return; // no coin
    } 
    // adjust
    int new_value = old_value;
    int rand = hall_luck_random_float(self, pr, 0.4, 100);
    if (rand > 90) {
        new_value += 52; 
    } else {
        new_value += 11;
    }
    if (new_value >= STATE_MAX_VALUE) {
        new_value = STATE_MAX_VALUE;
    }
    cdata->roles_state[typeid] = new_value;

    // pay
    uint32_t coin_old = cdata->coin;
    if (pay_coin > 0) {
        cdata->coin -= pay_coin;
    }
    int new_id = role_state_id(new_value);
    if (old_id != new_id) {
        hall_attribute_main(self->T, cdata); 
    }
    notify_adjust_result(s, pr, typeid, new_value, (new_value - old_value) > 16);
    hall_sync_money(s, pr);
    
    hall_playerdb_save(s, pr, true);
    hall_gamelog(s, self->charactionlog_handle, 
            "STATE,%u,%u,%u,%u,%u", sh_timer_now()/1000, cdata->accid,
            coin_old, old_value, new_value-old_value);
}

static void
refresh_state(struct module *s, struct player *pr, bool sync) {
    struct hall *self = MODULE_SELF;

    uint64_t now = sh_timer_now();
    struct chardata *cdata = &pr->data;
    if (cdata->last_state_refresh_time == 0) {
        cdata->last_state_refresh_time = now;
    }
   
    bool change_state = false;
    int i;
    for (i=0; i<ROLE_MAX; ++i) {
        if (cdata->ownrole[i]) {
            int state_value = cdata->roles_state[i];
            int old_id = role_state_id(state_value);
            if (old_id < ROLE_STATE_NORMAL) {
                state_value += min(20, (now - cdata->last_state_refresh_time) / 30);
                state_value = min(state_value, STATE_MAX_VALUE);
                cdata->roles_state[i] = state_value;
                int new_id = role_state_id(state_value);
                if (new_id != old_id) {
                    if (sync) {
                        hall_sync_state(s, pr, i, state_value);
                    }
                    change_state = true;
                }
            }
        }
    }
    if (change_state) {
        hall_attribute_main(self->T, &pr->data);
    }
    cdata->last_state_refresh_time = now;
}

static void
login(struct module *s, struct player* pr) {
    struct hall *self = MODULE_SELF;
    struct chardata* cdata = &pr->data;
    const struct role_tplt* tplt;
    if (cdata->role == 0) {
        tplt = NULL;
    } else {
        tplt = tplt_find(self->T, TPLT_ROLE, cdata->role);
    }
    if (tplt == NULL && 
        cdata->role != ROLE_DEF) {
        cdata->role = ROLE_DEF;
        tplt = tplt_find(self->T, TPLT_ROLE, cdata->role);
    }
    if (tplt) {
        if (!has_role(cdata, ROLE_DEF)) {
            add_role(cdata, ROLE_DEF);
        }
        use_role(cdata, tplt);
    } else {
        sh_error("can not found role %d, charid %u", cdata->role, cdata->charid);
    }

    refresh_state(s, pr, false);
}

void
hall_role_main(struct module *s, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_ENTERHALL:
        login(s, pr);
        break;
    case IDUM_USEROLE: {
        UM_CASTCK(UM_USEROLE, use, base, sz);
        process_userole(s, pr, use);
        break;
        }
    case IDUM_BUYROLE: {
        UM_CASTCK(UM_BUYROLE, buy, base, sz);
        process_buyrole(s, pr, buy);
        break;
        }
    case IDUM_ADJUSTSTATE: {
        UM_CASTCK(UM_ADJUSTSTATE, adjust, base, sz);
        process_adjust_state(s, pr, adjust);
        }
    }
}

static void
timecb(void *pointer, void *ud) {
    struct module *s = ud;
    struct player *pr = pointer;
    refresh_state(s, pr, true);
}

void
hall_role_time(struct module *s) {
    struct hall *self = MODULE_SELF;
    if (SEC_ELAPSED(60)) {
        sh_hash_foreach2(&self->acc2player, timecb, s);
    }
}
