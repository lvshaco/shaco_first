#include "sc_service.h"
#include "sc_util.h"
#include "sc.h"
#include "sc_log.h"
#include "sc_timer.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "player.h"
#include "playerdb.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// awardlogic
struct awardlogic {
    int db_handler;
    int rank_handler;
};

struct awardlogic*
awardlogic_create() {
    struct awardlogic* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
awardlogic_free(struct awardlogic* self) {
    free(self);
}

int
awardlogic_init(struct service* s) {
    struct awardlogic* self = SERVICE_SELF;
    if (sc_handler("playerdb", &self->db_handler) ||
        sc_handler("rank", &self->rank_handler))
        return 1;
    return 0;
}

static void
_levelup(uint32_t* exp, uint16_t* level) {
    const struct exp_tplt* tplt;
    uint32_t curexp = *exp;
    uint16_t curlv = *level;
    while (curexp > 0) {
        if (curlv >= LEVEL_MAX)
            break;
        tplt = tplt_find(TPLT_EXP, curlv+1);
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
_rank(struct awardlogic* self, struct player* p, 
      const char* type, const char* oldtype, uint64_t score) {
    struct service_message sm;
    sm.p1 = (void*)type;
    sm.p2 = (void*)oldtype;
    sm.i1 = p->data.charid;
    sm.n1 = score;
    service_notify_service(self->rank_handler, &sm);
}

static void
_award(struct awardlogic* self, 
       int8_t type, struct player* p, const struct memberaward* award) {
    struct chardata* cdata = &p->data;
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
        _levelup(&cdata->exp, &cdata->level);
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
            _rank(self, p, 
            _player_gradestr(new_grade), 
            _player_gradestr(old_grade),
            _get_score(cdata, cdata->score_normal));
            updated = true;
        } else if (award->score > cdata->score_normal) {
            cdata->score_normal = award->score;
            _rank(self, p, _player_gradestr(new_grade), "",
            _get_score(cdata, cdata->score_normal));
            updated = true;
        }
        break;
    case ROOM_TYPE_DASHI:
        if (award->score > 0) {
            cdata->score_dashi += award->score;
            _rank(self, p, "dashi", "", 
            _get_score(cdata, cdata->score_dashi));
            updated = true;
        }
        break;
    }
    if (updated) {
        send_playerdb(self->db_handler, p, PDB_SAVE);
    }
}

void
awardlogic_service(struct service* s, struct service_message* sm) {
    struct awardlogic* self = SERVICE_SELF;
    struct player** allp = sm->p1;
    struct memberaward* awards = sm->p2;
    int n = sm->i1;
    int8_t type = sm->i2;
    int i;
    for (i=0; i<n; ++i) {
        _award(self, type, allp[i], &awards[i]); 
    }
}
