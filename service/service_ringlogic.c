#include "sc_service.h"
#include "sc_util.h"
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

struct ringlogic {
    int dbhandler;
};

struct ringlogic*
ringlogic_create() {
    struct ringlogic* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
ringlogic_free(struct ringlogic* self) {
    free(self);
}

int
ringlogic_init(struct service* s) {
    struct ringlogic* self = SERVICE_SELF;
    if (sc_handler("playerdb", &self->dbhandler))
        return 1;
    SUBSCRIBE_MSG(s->serviceid, IDUM_RINGPAGEBUY);
    SUBSCRIBE_MSG(s->serviceid, IDUM_RINGPAGERENAME);
    SUBSCRIBE_MSG(s->serviceid, IDUM_RINGEQUIP);
    SUBSCRIBE_MSG(s->serviceid, IDUM_RINGSALE);
    return 0;
}

/////////////////////////////////////////////////////////////////////

static inline bool
_hasdb() {
    return sc_node_get(HNODE_ID(NODE_REDISPROXY, 0)) != NULL;
}

static inline const struct ring_tplt* 
_ringtplt(uint32_t ringid) {
    const struct tplt_visitor* vist = tplt_get_visitor(TPLT_RING);
    if (vist) {
        return tplt_visitor_find(vist, ringid);
    }
    return NULL;
}

static void
_sync_money(struct player* p) {
    UM_DEFFORWARD(fw, p->cid, UM_SYNCMONEY, sm);
    sm->coin = p->data.coin;
    sm->diamond = p->data.diamond;
    _forward_toplayer(p, fw);
}

static void
_sync_ringpage(struct player* p) {
    UM_DEFFORWARD(fw, p->cid, UM_RINGPAGESYNC, sync);
    sync->curpage = p->data.ringdata.npage;
    _forward_toplayer(p, fw);
}

static struct ringpage*
_get_ringpage(struct ringdata* data, uint8_t index) {
    if (index < data->npage)
        return &data->pages[index];
    else
        return NULL;
}

static struct ringobj*
_get_ringobj(struct ringdata* data, uint32_t id) {
    struct ringobj* robj;
    uint8_t i;
    for (i=0; i<min(data->nring, RING_MAX); ++i) {
        robj = &data->rings[i];
        if (robj->ringid == id) {
            return robj;
        }
    }
    return NULL;
}

/*
static struct ringobj*
_add_ringobj(struct ringdata* data, uint32_t id, uint8_t n) {
    assert(data->nring < RING_MAX);
    struct ringobj* robj = &data->rings[data->nring++];
    robj->ringid = id;
    robj->stack = n;
    return obj;
}

static uint8_t
_stack_ringobj(struct ringobj* robj, uint8_t n) {
    uint8_t stack = robj->stack + n;
    if (stack > RING_STACK || 
        stack < robj->stack) { 
        robj->stack = RING_STACK;
    } else {
        robj->stack = stack;
    }
    return robj->stack;
}

static uint8_t 
_reduce_ringobj(struct ringobj* robj, uint8_t n) {
    if (robj->stack >= n)
        robj->stack -= n;
    else
        robj->stack = 0;
    return robj->stack;
}
*/
static void
_handle_equipring(struct ringlogic* self, struct player_message* pm) {
    UM_CAST(UM_RINGEQUIP, um, pm->um);
    
    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;

    struct ringpage* page = _get_ringpage(rdata, um->index);
    if (page == NULL) {
        return;
    }
    struct ringobj* robj;
    uint8_t i;
    for (i=0; i<RING_PAGE_SLOT; ++i) {
        if (um->rings[i] > 0) {
            robj = _get_ringobj(rdata, um->rings[i]);
            if (robj == NULL || robj->stack == 0)
                return; 
        } 
    }
   
    if (!_hasdb()) {
        return; // NO DB
    }
    // do logic
    memcpy(page->slots, um->rings, sizeof(page->slots));
    player_send_dbcmd(self->dbhandler, p, PDB_SAVE);
}
/*
static void
_handle_salering(struct ringlogic* self, struct player_message* pm) {
    UM_CAST(UM_RINGSALE, um, pm->um);
    
    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
}
*/

static void
_handle_renameringpage(struct ringlogic* self, struct player_message* pm) {
    UM_CAST(UM_RINGPAGERENAME, um, pm->um);
    
    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;

    if (um->index >= rdata->npage) {
        return; // 该页不存在
    } 
    if (!_hasdb()) {
        return; // NO DB
    }
    // do logic
    struct ringpage* page = &rdata->pages[um->index];
    strncpy(page->name, um->name, sizeof(page->name));
    
    player_send_dbcmd(self->dbhandler, p, PDB_SAVE);
}

static void
_handle_buyringpage(struct ringlogic* self, struct player_message* pm) {
    struct player* p = pm->p;
    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;

    if (rdata->npage >= RING_PAGE_MAX) {
        return; // 最大页
    }
    if (cdata->diamond < RING_PAGE_PRICE) {
        return; // 钻石不足
    }
    if (!_hasdb()) {
        return; // NO DB
    }
    // do logic
    rdata->npage += 1;
    cdata->diamond -= RING_PAGE_PRICE;
    
    _sync_money(p);
    _sync_ringpage(p); 
    player_send_dbcmd(self->dbhandler, p, PDB_SAVE);
    return;
}

void
ringlogic_usermsg(struct service* s, int id, void* msg, int sz) {
    struct ringlogic* self = SERVICE_SELF;
    struct player_message* pm = msg;
    switch (pm->um->msgid) {
    case IDUM_RINGPAGEBUY:
        _handle_buyringpage(self, pm);
        break;
    case IDUM_RINGPAGERENAME:
        _handle_renameringpage(self, pm);
        break;
    case IDUM_RINGEQUIP:
        _handle_equipring(self, pm);
        break;
    //case IDUM_RINGSALE:
        //_handle_salering(self, pm);
        //break;
    }
}

static void
_onlogin(struct player* p) {
    struct chardata* cdata = &p->data;
    struct ringdata* rdata = &cdata->ringdata;
    if (rdata->npage > RING_PAGE_MAX)
        rdata->npage = RING_PAGE_MAX;
    if (rdata->nring > RING_MAX)
        rdata->nring = RING_MAX;
}

void
ringlogic_service(struct service* s, struct service_message* sm) {
    //struct ringlogic* self = SERVICE_SELF;
    switch (sm->type) {
    case WE_LOGIN: {
        struct player* p = sm->msg;
        _onlogin(p);
        }
        break;
    }
}
