#include "sc_service.h"
#include "sc_env.h"
#include "sc.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "sc_log.h"
#include "sc_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include "player.h"
#include "worldhelper.h"
#include "worldevent.h"
#include "util.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

