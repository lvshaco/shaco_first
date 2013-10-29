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
#include "worldhelper.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct world {
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
_build_chardata(struct chardata* data, uint32_t charid) {
    // todo: this just for test 
    memset(data, 0, sizeof(*data));
    data->charid = charid;
    snprintf(data->name, sizeof(data->name), "wabao-n%u", charid);
    data->level = 1;
    data->exp = 0;
    data->coin = 1388888;
    data->diamond = 88888;
    data->package = 10;
    data->role = 1;
    data->skin = 1;
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_ROLEDATA);
    const struct roledata_tplt* role = tplt_visitor_find(vist, data->role);
    if (role == NULL) {
        host_error("can not found role %d, charid %u", data->role, data->charid);
        return;
    }
    data->oxygen = role->oxygen;
    data->body = role->body;
    data->quick = role->quick;
}

static void 
_login(struct world* self, struct node_message* nm) {
    UM_CAST(UM_FORWARD, req, nm->um);
    const struct host_node* node = nm->hn;
    int cid = req->cid;

    struct player* p;
    p = _getplayer(node->sid, cid);
    if (p != NULL) {
        _forward_logoutplayer(node, cid, LOGOUT_RELOGIN);
        return;
    }
    p = _allocplayer(node->sid, cid);
    if (p == NULL) {
        _forward_logoutplayer(node, cid, LOGOUT_FULL);
        return;
    }
    p->status = PS_LOGIN;
    uint32_t charid = self->chariditer++; 
    _build_chardata(&p->data, charid);
    p->status = PS_GAME;
    
    if (_hashplayer(p)) {
        _forward_logoutplayer(node, cid, LOGOUT_FULL);
        _freeplayer(p);
        return;
    }
    UM_DEFFORWARD(fw, cid, UM_CHARINFO, ci);
    ci->data = p->data;
    UM_SENDFORWARD(node->connid, fw);
}

static void
_logout(struct world* self, struct player* p) {
    _freeplayer(p);
    //struct gate_message gm;
    //gm.c = c;
    //gm.msg = nm;
    //service_notify_net(self->handler, (void*)&gm);
    //host_gate_disconnclient(c, false);
}

static void 
_handlegate(struct world* self, struct node_message* nm) {
    hassertlog(nm->um->msgid == IDUM_FORWARD);

    UM_CAST(UM_FORWARD, fw, nm->um);
    switch (fw->wrap.msgid) {
    case IDUM_LOGIN:
        _login(self, nm);
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
