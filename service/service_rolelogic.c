#include "sc_service.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "worldhelper.h"
#include "worldevent.h"
#include "player.h"
#include "playerdb.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <string.h>

#define ROLE_DEF 10 // 默认给予ID

struct rolelogic {
    int dbhandler;
};

struct rolelogic*
rolelogic_create() {
    struct rolelogic* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
rolelogic_free(struct rolelogic* self) {
    free(self);
}

int
rolelogic_init(struct service* s) {
    struct rolelogic* self = SERVICE_SELF;
    self->dbhandler = service_query_id("playerdb");
    if (self->dbhandler == SERVICE_INVALID) {
        sc_error("lost playerdb service");
        return 1;
    }
    SUBSCRIBE_MSG(s->serviceid, IDUM_USEROLE);
    SUBSCRIBE_MSG(s->serviceid, IDUM_BUYROLE);
    return 0;
}

/////////////////////////////////////////////////////////////////////

static inline bool
_hasdb() {
    return sc_node_get(HNODE_ID(NODE_REDISPROXY, 0)) != NULL;
}

static inline const struct role_tplt* 
_roletplt(uint32_t roleid) {
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_ROLE);
    if (vist) {
        return tplt_visitor_find(vist, roleid);
    }
    return NULL;
}

static bool
_hasrole(struct chardata* cdata, uint32_t roleid) {
    uint32_t typeid = ROLE_TYPEID(roleid);
    uint32_t clothid = ROLE_CLOTHID(roleid);
    if (typeid >= 0 && typeid < ROLE_MAX) {
        if (clothid >= 0 && clothid < ROLE_CLOTHES_MAX) {
            return cdata->ownrole[typeid] & (1<<clothid);
        }
    }
    return false;
}

static int
_addrole(struct chardata* cdata, uint32_t roleid) {
    uint32_t typeid = ROLE_TYPEID(roleid);
    uint32_t clothid = ROLE_CLOTHID(roleid);
    if (typeid >= 0 && typeid < ROLE_MAX) {
        if (clothid >= 0 && clothid < ROLE_CLOTHES_MAX) {
            cdata->ownrole[typeid] |= 1<<clothid;
            return 0;
        }
    }
    return 0;
}

static void
_sync_role(struct player* p) {
    UM_DEFFORWARD(fw, p->cid, UM_CHARINFO, ci);
    ci->data = p->data;
    _forward_toplayer(p, fw);
}

static void
_sync_money(struct player* p) {
    UM_DEFFORWARD(fw, p->cid, UM_SYNCMONEY, sm);
    sm->coin = p->data.coin;
    sm->diamond = p->data.diamond;
    _forward_toplayer(p, fw);
}

static void
_sync_addrole(struct player* p, uint32_t roleid) {
    UM_DEFFORWARD(fw, p->cid, UM_ADDROLE, ar);
    ar->roleid = roleid;
    _forward_toplayer(p, fw);
}

static void
_userole(struct chardata* cdata, const struct role_tplt* tplt) {
    cdata->role = tplt->id;
    
    cdata->oxygen = tplt->oxygen;
    cdata->body = tplt->body;
    cdata->quick = tplt->quick;
}

static void
_handle_userole(struct rolelogic* self, struct player_message* pm) {
    UM_CAST(UM_USEROLE, um, pm->um);
    
    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
    uint32_t roleid  = um->roleid;

    const struct role_tplt* tplt = _roletplt(roleid);
    if (tplt == NULL) {
        return;
    }
    if (!_hasrole(cdata, roleid)) {
        return;
    }
    if (!_hasdb()) {
        return; // NO DB
    }
    // do logic
    _userole(cdata, tplt);
    _sync_role(p);
    player_send_dbcmd(self->dbhandler, p, PDB_SAVE);
}

static void
_handle_buyrole(struct rolelogic* self, struct player_message* pm) {
    UM_CAST(UM_BUYROLE, um, pm->um);

    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
    uint32_t roleid  = um->roleid;
   
    if (_hasrole(cdata, roleid)) {
        return; // 已经拥有
    }
    const struct role_tplt* tplt = _roletplt(roleid);
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
    if (!_hasdb()) {
        return; // NO DB
    }
    // do logic
    if (_addrole(cdata, roleid)) {
        return;
    }
    cdata->coin -= tplt->needcoin;
    cdata->diamond -= tplt->needdiamond;
    _sync_addrole(p, roleid);
    _sync_money(p);
    player_send_dbcmd(self->dbhandler, p, PDB_SAVE);
    return;
}

void
rolelogic_usermsg(struct service* s, int id, void* msg, int sz) {
    struct rolelogic* self = SERVICE_SELF;
    struct player_message* pm = msg;
    switch (pm->um->msgid) {
    case IDUM_USEROLE:
        _handle_userole(self, pm);
        break;
    case IDUM_BUYROLE:
        _handle_buyrole(self, pm);
        break;
    }
}

static void
_onlogin(struct player* p) {
    struct chardata* cdata = &p->data;
    const struct role_tplt* tplt;
    if (cdata->role == 0) {
        tplt = NULL;
    } else {
        tplt = _roletplt(cdata->role);
    }
    if (tplt == NULL && 
        cdata->role != ROLE_DEF) {
        cdata->role = ROLE_DEF;
        tplt = _roletplt(cdata->role);
    }
    if (tplt) {
        if (!_hasrole(cdata, ROLE_DEF)) {
            _addrole(cdata, ROLE_DEF);
        }
        _userole(cdata, tplt);
    } else {
        sc_error("can not found role %d, charid %u", cdata->role, cdata->charid);
    }
}

void
rolelogic_service(struct service* s, struct service_message* sm) {
    //struct rolelogic* self = SERVICE_SELF;
    switch (sm->type) {
    case WE_LOGIN: {
        struct player* p = sm->msg;
        _onlogin(p);
        }
        break;
    }
}
