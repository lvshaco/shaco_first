#include "host_service.h"
#include "host_assert.h"
#include "host.h"
#include "host_timer.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include "player.h"
#include "playerdb.h"
#include "worldhelper.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct world {
    uint32_t dbhandler;
    uint32_t chariditer;
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

    tplt_fini();
}

static int
_loadtplt() {

#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ROLEDATA, sizeof(struct roledata_tplt), TBLFILE(roledata), TPLT_VIST_VEC32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
}

int
world_init(struct service* s) {
    struct world* self = SERVICE_SELF;

    self->dbhandler = service_query_id("playerdb");
    if (self->dbhandler == SERVICE_INVALID) {
        host_error("lost playerdb service");
        return 1;
    }
    if (_loadtplt()) {
        return 1;
    }
    self->chariditer = 1;
    int cmax = host_getint("world_cmax_pergate", 0);
    int hmax = host_getint("world_hmax_pergate", cmax);
    int gmax = host_getint("world_gmax", 0);
    _allocplayers(cmax, hmax, gmax);
    SUBSCRIBE_MSG(s->serviceid, IDUM_FORWARD); 

    host_timer_register(s->serviceid, 1000);
    return 0;
}

static void
_onlogin(struct player* p) {
    p->status = PS_GAME;

    struct chardata* data = &p->data;
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_ROLEDATA);
    const struct roledata_tplt* role = tplt_visitor_find(vist, data->role);
    if (role == NULL) {
        host_error("can not found role %d, charid %u", data->role, data->charid);
        return;
    }
    data->oxygen = role->oxygen;
    data->body = role->body;
    data->quick = role->quick;

    if (_hashplayer(p)) {
        _forward_logout(p, SERR_WORLDFULL);
        _freeplayer(p);
        return;
    }
    UM_DEFFORWARD(fw, p->cid, UM_CHARINFO, ci);
    ci->data = p->data;
    _forward_toplayer(p, fw);
}

void
world_service(struct service* s, struct service_message* sm) {
    assert(sm->sz == sizeof(struct playerdbres));
    struct playerdbres* res = sm->msg;
    struct player* p = res->p;
    switch (res->error) {
    case SERR_OK:
        if (p->status == PS_LOGIN) {
            _onlogin(p);
        }
        break;
    case SERR_NOCHAR:
        _forward_loginfail(p, res->error);
        break;
    default:
        _forward_logout(p, res->error);
        _freeplayer(p);
        break;
    }
}


static int
_dbcmd(struct world* self, struct player* p, int8_t type) {
    struct service_message sm;
    sm.sessionid = 0;
    sm.source = 0;
   
    struct playerdbcmd cmd;
    cmd.type = type;
    cmd.p = p;
    cmd.err = 1;
    sm.msg = &cmd;
    sm.sz = sizeof(struct playerdbcmd);
    service_notify_service(self->dbhandler, &sm);
    return cmd.err;
}

static void 
_login(struct world* self, const struct host_node* node, int cid, struct UM_BASE* um) {
    UM_CAST(UM_LOGIN, lo, um);

    struct player* p;
    p = _getplayer(node->sid, cid);
    if (p != NULL) {
        _forward_logout(p, SERR_RELOGIN);
        _freeplayer(p);
        return;
    }
    // todo: check repeat account

    p = _allocplayer(node->sid, cid);
    if (p == NULL) {
        _forward_logout(p, SERR_WORLDFULL);
        return;
    }
    
    p->data.accid = lo->accid;
    if (_dbcmd(self, p, PDB_QUERY)) {
        _forward_logout(p, SERR_NODB);
        _freeplayer(p);
        return;
    }
}

static void
_createchar(struct world* self, const struct host_node* node, int cid, struct UM_BASE* um) {
    UM_CAST(UM_CHARCREATE, cre, um);

    struct player* p;
    p = _getplayer(node->sid, cid);
    if (p == NULL) {
        return;
    }
    if (p->status == PS_WAITCREATECHAR) {
        int len = sizeof(p->data.name) - 1;
        memcpy(p->data.name, cre->name, len);
        p->data.name[len] = '\0';
        if (_dbcmd(self, p, PDB_CHECKNAME)) {
            _forward_logout(p, SERR_NODB);
            _freeplayer(p);
        }
    }
}

static void
_logout(struct world* self, struct player* p) {
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
        host_dispatcher_usermsg(&pm, 0);
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
