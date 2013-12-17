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

struct awardlogic {
    int db_handler;
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
    if (sc_handler("playerdb", &self->db_handler))
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

static void
_award(struct awardlogic* self, 
       int8_t type, struct player* p, const struct memberaward* award) {
    struct chardata* cdata = &p->data;
    bool updated = false;
    if (award->coin > 0) {
        cdata->coin += award->coin;
        updated = true;
    }
    if (award->score > 0) {
        if (type == ROOM_TYPE_DASHI) {
            cdata->score_dashi += award->score;
            updated = true;
        } else if (award->score > cdata->score_normal) {
            cdata->score_normal = award->score;
        }
    }
    if (award->exp > 0) {
        sc_limitadd(award->exp, &cdata->exp, UINT_MAX);
        _levelup(&cdata->exp, &cdata->level);
        updated = true;
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
