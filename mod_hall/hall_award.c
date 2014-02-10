#include "sc_service.h"
#include "sc_node.h"
#include "sh_util.h"
#include "sc.h"
#include "sc_log.h"
#include "sc_timer.h"
#include "hall_player.h"
#include "hall_playerdb.h"
#include "msg_server.h"
#include "hall.h"
#include "hall_tplt.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void
_levelup(struct hall *self, uint32_t* exp, uint16_t* level) {
    const struct exp_tplt* tplt;
    uint32_t curexp = *exp;
    uint16_t curlv = *level;
    while (curexp > 0) {
        if (curlv >= LEVEL_MAX)
            break;
        tplt = tplt_find(self->T, TPLT_EXP, curlv+1);
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
_rank(struct service *s, struct player* pr, 
      const char* type, const char* type_old, uint64_t score) {
    struct hall *self = SERVICE_SELF;

    // todo do not pointer
    UM_DEFFIX(UM_DBRANK, dr);
    dr->type = type;
    dr->type_old = type_old;
    dr->charid = pr->data.charid;
    dr->score = score;
    sh_service_send(SERVICE_ID, self->rank_handle, MT_UM, dr, sizeof(*dr));
}

static void
process_award(struct service *s, struct player* pr, int8_t type, const struct memberaward* award) {
    struct hall *self = SERVICE_SELF;
    struct chardata* cdata = &pr->data;
    bool updated = false;
    // coin
    if (award->coin > 0) {
        cdata->coin += award->coin;
        updated = true;
    }
    // exp
    int old_grade = 0, new_grade = 0;
    if (award->exp > 0) {
        sc_limitadd(award->exp, &cdata->exp, UINT_MAX);
        uint16_t old_level = cdata->level;
        _levelup(self, &cdata->exp, &cdata->level);
        if (old_level != cdata->level){
            old_grade = _player_gradeid(old_level);
            new_grade = _player_gradeid(cdata->level);
        }
        updated = true;
    }
    // score
    switch (type) {
    case ROOM_TYPE_NORMAL:
        if (new_grade != old_grade) {
            cdata->score_normal = award->score;
            _rank(s, pr, 
            _player_gradestr(new_grade), 
            _player_gradestr(old_grade),
            _get_score(cdata, cdata->score_normal));
            updated = true;
        } else if (award->score > cdata->score_normal) {
            cdata->score_normal = award->score;
            _rank(s, pr, _player_gradestr(new_grade), "",
            _get_score(cdata, cdata->score_normal));
            updated = true;
        }
        break;
    case ROOM_TYPE_DASHI: {
        int score = (int)cdata->score_dashi + award->score;
        if (score < 0)
            score = 0;
        cdata->score_dashi = score;
        _rank(s, pr, "dashi", "", 
        _get_score(cdata, cdata->score_dashi));
        updated = true;
        }
        break;
    }
    if (updated) { 
        hall_playerdb_send(s, pr, PDB_SAVE);
    }
}

void
hall_award_main(struct service *s, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_GAMEAWARD: {
        UM_CASTCK(UM_GAMEAWARD, ga, base, sz);
        process_award(s, pr, ga->type, &ga->award);
        break;
        }
    }
}
