#include "host_service.h"
#include "host.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include "player.h"
#include "worldhelper.h"
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
}
int
world_init(struct service* s) {
    //struct world* self = SERVICE_SELF;
    int cmax = host_getint("world_cmax_pergate", 0);
    int hmax = host_getint("world_hmax_pergate", cmax);
    int gmax = host_getint("world_gmax", 0);
    _allocplayers(cmax, hmax, gmax);
    SUBSCRIBE_MSG(s->serviceid, UMID_FORWARD);
    return 0;
}
void
world_service(struct service* s, struct service_message* sm) {
    //struct world* self = SERVICE_SELF;
}

static void 
_login_req(struct world* self, const struct host_node* node, int cid, struct UM_base* um) {
    struct player* p;
    p = _getplayer(node->sid, cid);
    if (p != NULL) {
        _notify_logout(node, cid, LOGOUT_RELOGIN);
        return;
    }
    p = _allocplayer(node->sid, cid);
    if (p == NULL) {
        _notify_logout(node, cid, LOGOUT_FULL);
        return;
    }
    p->status = PS_LOGIN;

    uint32_t charid = ++self->chariditer;
    // todo: this just for test
    struct chardata* data = &p->data;
    data->charid = charid;
    snprintf(data->name, sizeof(data->name), "wabao-n%u", charid);
    if (_hashplayer(p)) {
        _notify_logout(node, cid, LOGOUT_FULL);
        _freeplayer(p);
        return;
    }
    UM_FORWARD(fw, cid, UM_charinfo, ci, UMID_CHARINFO);
    ci->data = *data;
    UM_SENDFORWARD(node->connid, fw);
}
static void
_logout_req(struct world* self, struct player* p) {
    _freeplayer(p);
}
static void 
_handlegate(struct world* self, struct node_message* nm) {
    assert(nm->um->msgid == UMID_FORWARD);
    UM_CAST(UM_forward, fw, nm->um);

    const struct host_node* hn = nm->hn;
    struct UM_base* m = &fw->wrap;
    switch (m->msgid) {
    case UMID_LOGIN:
        _login_req(self, hn, fw->cid, m);
        break;
    default: {
        struct player* p = _getplayer(hn->sid, fw->cid);
        if (p == NULL) {
            _notify_logout(hn, fw->cid, LOGOUT_NOLOGIN);
            return;
        }
        switch (m->msgid) {
        case UMID_LOGOUT:
            _logout_req(self, p);
            break;
        }
        break;
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
