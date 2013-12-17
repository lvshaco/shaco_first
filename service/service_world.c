#include "sc_service.h"
#include "sc_env.h"
#include "sc_assert.h"
#include "sc.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include "player.h"
#include "playerdb.h"
#include "worldhelper.h"
#include "worldevent.h"
#include "util.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct world {
    int dbhandler;
    int rolehandler;
    int ringhandler;
    int attrihandler;
};

struct world*
world_create() {
    struct world* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
world_free(struct world* self) {
    if (self == NULL)
        return;
    _freeplayers();
    free(self);
}

int
world_init(struct service* s) {
    struct world* self = SERVICE_SELF;

    if (sc_handler("rolelogic", &self->rolehandler) ||
        sc_handler("ringlogic", &self->ringhandler) ||
        sc_handler("attribute", &self->attrihandler) ||
        sc_handler("playerdb", &self->dbhandler)) {
        return 1;
    }
        
    int cmax = sc_getint("world_cmax_pergate", 0);
    int hmax = sc_getint("world_hmax_pergate", cmax);
    int gmax = sc_getint("world_gmax", 0);
    _allocplayers(cmax, hmax, gmax);
    SUBSCRIBE_MSG(s->serviceid, IDUM_FORWARD); 

    sc_timer_register(s->serviceid, 1000);
    return 0;
}

static void
_onlogin(struct world* self, struct player* p) {
    p->status = PS_GAME;

    struct chardata* cdata = &p->data;
   
    struct service_message sm = { 0, 0, WE_LOGIN, sizeof(p), p };
    service_notify_service(self->rolehandler, &sm);
    service_notify_service(self->ringhandler, &sm);

    // refresh attribute
    struct service_message sm2 = { 0, 0, 0, sizeof(p), p };
    service_notify_service(self->attrihandler, &sm2);
    
    UM_DEFFORWARD(fw, p->cid, UM_CHARINFO, ci);
    ci->data = *cdata;
    _forward_toplayer(p, fw);
}

void
world_service(struct service* s, struct service_message* sm) {
    struct world* self = SERVICE_SELF;
    struct player* p = sm->msg;
    int err = sm->type;
    switch (err) {
    case SERR_OK:
        if (p->status == PS_LOGIN) {
            _onlogin(self, p);
        }
        break;
    case SERR_NOCHAR:
    case SERR_NAMEEXIST:
        _forward_loginfail(p, err);
        break;
    default:
        _forward_logout(p, err);
        _freeplayer(p);
        break;
    }
}

static void 
_login(struct world* self, const struct sc_node* node, int cid, struct UM_BASE* um) {
    UM_CAST(UM_LOGIN, lo, um);
    uint32_t accid = lo->accid;
    struct player* p;
    struct player* other;

    p = _getplayer(node->sid, cid);
    if (p != NULL) {
        _forward_connlogout(node, cid, SERR_RELOGIN);
        _freeplayer(p);
        return;
    }
    other = _getplayerbyaccid(accid);
    if (other) {
        _forward_connlogout(node, cid, SERR_ACCLOGINED);
        return;
    }
    p = _allocplayer(node->sid, cid);
    if (p == NULL) {
        _forward_connlogout(node, cid, SERR_WORLDFULL);
        return;
    }
    if (_hashplayeracc(p, accid)) {
        _forward_connlogout(node, cid, SERR_WORLDFULL);
        _freeplayer(p);
        return;
    }
    if (send_playerdb(self->dbhandler, p, PDB_QUERY)) {
        _forward_connlogout(node, cid, SERR_NODB);
        _freeplayer(p);
        return;
    }
}

static void
_createchar(struct world* self, const struct sc_node* node, int cid, struct UM_BASE* um) {
    UM_CAST(UM_CHARCREATE, cre, um);

    struct player* p;
    p = _getplayer(node->sid, cid);
    if (p == NULL) {
        return;
    }
    if (p->status == PS_WAITCREATECHAR) {
        strncpychk(p->data.name, sizeof(p->data.name), cre->name, sizeof(cre->name));
        if (send_playerdb(self->dbhandler, p, PDB_CHECKNAME)) {
            _forward_logout(p, SERR_NODB);
            _freeplayer(p);
        }
    }
}

static void
_logout(struct world* self, struct player* p) {
    send_playerdb(self->dbhandler, p, PDB_SAVE);
    _freeplayer(p);
}

static void 
_handlegate(struct world* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_FORWARD);

    UM_CAST(UM_FORWARD, fw, nm->um);
    switch (fw->wrap.msgid) {
    case IDUM_LOGIN:
        _login(self, nm->hn, fw->cid, &fw->wrap);
        break;
    case IDUM_CHARCREATE:
        _createchar(self, nm->hn, fw->cid, &fw->wrap);
        break;
    default: {
        struct player_message pm;
        if (_decode_playermessage(nm, &pm)) {
            return;
        }
        sc_dispatcher_usermsg(&pm, 0);
        switch (pm.um->msgid) {
        case IDUM_LOGOUT:
            _logout(self, pm.p);
            break;
        }
        }
    }
}

void
world_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct world* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_GATE:
        _handlegate(self, &nm);
        break;
    }
}

void
world_time(struct service* s) {
    //struct world* self= SERVICE_SELF;
}
