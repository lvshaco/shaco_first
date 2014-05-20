#include "sh.h"
#include "hall.h"
#include "hall_tplt.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "hall_attribute.h"
#include "msg_server.h"

static void
_levelup(struct tplt *T, uint32_t* exp, uint16_t* level) {
    const struct exp_tplt* tplt;
    uint32_t curexp = *exp;
    uint16_t curlv = *level;
    while (curexp > 0) {
        if (curlv >= LEVEL_MAX)
            break;
        tplt = tplt_find(T, TPLT_EXP, curlv+1);
        if (tplt == NULL)
            break;
        if (curexp < tplt->curexp)
            break;
        curexp -= tplt->curexp;
        curlv++;
    }
    *exp = curexp;
    *level = curlv;
}

static inline uint64_t
_get_score(struct chardata* cdata, uint32_t score) {
    uint64_t level = cdata->level;
    if (level > 999) level = 999;
    uint32_t exp = cdata->exp;
    if (exp > 9999999) exp = 9999999;
    return (uint64_t)score * 10000000000L + 
    (uint64_t)cdata->level * 10000000 + cdata->exp;
}

static inline void
_rank(struct module *s, struct player* pr, 
      const char* type, const char* type_old, uint64_t score) {
    struct hall *self = MODULE_SELF;

    // todo do not pointer
    UM_DEFFIX(UM_DBRANK, dr);
    dr->type = type;
    dr->type_old = type_old;
    dr->charid = pr->data.charid;
    dr->score = score;
    sh_handle_send(MODULE_ID, self->rank_handle, MT_UM, dr, sizeof(*dr));
}

static inline int
_take_state(struct module *s, struct player *pr, int take) {
    struct hall *self = MODULE_SELF;
    struct chardata* cdata = &pr->data;

    uint32_t typeid = ROLE_TYPEID(cdata->role);
    if (!IS_VALID_TYPEID(typeid)) {
        return 1;
    }
    int old_value = cdata->roles_state[typeid];
    int new_value = old_value;
    if (new_value > take) {
        new_value -= take;
    } else {
        new_value = 0;
    }
    cdata->roles_state[typeid] = new_value;
    int old_id = role_state_id(old_value);
    int new_id = role_state_id(new_value);
    if (old_id != new_id) {
        hall_attribute_main(self->T, cdata);
        hall_sync_state(s, pr, typeid, new_value);
    }
    return 0;
}

static void
process_award(struct module *s, struct player* pr, int8_t type, const struct memberaward* award) {
    struct hall *self = MODULE_SELF;
    struct chardata* cdata = &pr->data;
    bool updated = false;
    // take state
    if (award->take_state > 0) {
        if (!_take_state(s, pr, award->take_state)) {
            updated = true;
        }
    }
    // coin
    if (award->coin > 0) {
        cdata->coin += award->coin;
        hall_sync_money(s, pr);
        updated = true;
    }
    // exp
    int old_grade = 0, new_grade = 0;
    if (award->exp > 0) {
        sh_limitadd(award->exp, &cdata->exp, UINT_MAX);
        uint16_t old_level = cdata->level;
        _levelup(self->T, &cdata->exp, &cdata->level);
        if (old_level != cdata->level){
            old_grade = _player_gradeid(old_level);
            new_grade = _player_gradeid(cdata->level);
        }
        hall_sync_exp(s, pr);
        updated = true;
    }
    // score_normal
    if (award->score_normal > 0) {
        if (new_grade != old_grade) {
            cdata->score_normal = award->score_normal;
            _rank(s, pr, 
            _player_gradestr(new_grade), 
            _player_gradestr(old_grade),
            _get_score(cdata, cdata->score_normal));
            updated = true;
        } else if (award->score_normal > cdata->score_normal) {
            cdata->score_normal = award->score_normal;
            _rank(s, pr, _player_gradestr(new_grade), "",
            _get_score(cdata, cdata->score_normal));
            updated = true;
        }
    }
    // score_dashi
    if (award->score_dashi != 0) {
        int score = (int)cdata->score_dashi + award->score_dashi;
        if (score < 0)
            score = 0;
        cdata->score_dashi = score;
        _rank(s, pr, "dashi", "", 
        _get_score(cdata, cdata->score_dashi));
        updated = true;
    }
    // luck_factor
    if (award->luck_factor != cdata->luck_factor) {
        cdata->luck_factor = award->luck_factor;
        updated = true;
    }
    if (updated) { 
        hall_playerdb_save(s, pr, true);
    }
}

void
hall_award_main(struct module *s, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_GAMEAWARD: {
        UM_CASTCK(UM_GAMEAWARD, ga, base, sz);
        process_award(s, pr, ga->type, &ga->award);
        break;
        }
    }
}
