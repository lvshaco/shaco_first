#include "ringlogic.h"
#include "sc_service.h"
#include "sh_util.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "player.h"
#include "playerdb.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "user_message.h"
#include "cli_message.h"
#include "attrilogic.h"
#include <string.h>

#define RING_NPAGE_INIT 1
/////////////////////////////////////////////////////////////////////

static void
sync_money(struct service *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_SYNCMONEY, sm);
    cl->uid = UID(pr);
    sm->coin = pr->data.coin;
    sm->diamond = pr->data.diamond;
    sh_service_send(SERVICE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*sm));
}

static void
sync_ringpage(struct service *s, struct player* pr) {
    UM_DEFWRAP(UM_CLIENT, cl, UM_RINGPAGESYNC, sy);
    cl->uid = UID(pr);
    sy->curpage = pr->data.ringdata.npage;
    sh_service_send(SERVICE_ID, pr->watchdog_source, MT_UM, cl, sizeof(*cl) + sizeof(*sy));
}

static struct ringpage*
get_ringpage(struct ringdata* data, uint8_t index) {
    if (index < data->npage)
        return &data->pages[index];
    else
        return NULL;
}

static struct ringobj*
get_ringobj(struct ringdata* data, uint32_t id) {
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
add_ringobj(struct ringdata* data, uint32_t id, uint8_t n) {
    assert(data->nring < RING_MAX);
    struct ringobj* robj = &data->rings[data->nring++];
    robj->ringid = id;
    robj->stack = n;
    return obj;
}

static uint8_t
stack_ringobj(struct ringobj* robj, uint8_t n) {
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
reduce_ringobj(struct ringobj* robj, uint8_t n) {
    if (robj->stack >= n)
        robj->stack -= n;
    else
        robj->stack = 0;
    return robj->stack;
}
*/
static void
process_equipring(struct service *s, struct player *pr, const struct UM_RINGEQUIP *um) {
    struct chardata* cdata = &pr->data;
    struct ringdata* rdata = &cdata->ringdata;

    struct ringpage* page = get_ringpage(rdata, um->index);
    if (page == NULL) {
        return;
    }
    struct ringobj* robj;
    uint8_t i;
    for (i=0; i<RING_PAGE_SLOT; ++i) {
        if (um->rings[i] > 0) {
            robj = get_ringobj(rdata, um->rings[i]);
            if (robj == NULL || robj->stack == 0)
                return; 
        } 
    }
    if (!memcmp(page->slots, um->rings, sizeof(page->slots))) {
        return;
    }
    // do logic
    memcpy(page->slots, um->rings, sizeof(page->slots));
    playerdb_send(s, pr, PDB_SAVE);
}
/*
static void
process_salering(struct ringlogic* self, struct player_message* pm) {
    UM_CAST(UM_RINGSALE, um, pm->um);
    
    struct player* pr = pm->pr;
    struct chardata* cdata = &pr->data;
}
*/

static void
process_renameringpage(struct service *s, struct player *pr, const struct UM_RINGPAGERENAME *um) {
    struct chardata* cdata = &pr->data;
    struct ringdata* rdata = &cdata->ringdata;

    if (um->index >= rdata->npage) {
        return; // 该页不存在
    } 
    struct ringpage* page = &rdata->pages[um->index];
    if (!strncmp(page->name, um->name, sizeof(page->name))) {
        return;
    }
    // do logic
    strncpy(page->name, um->name, sizeof(page->name));
    
    playerdb_send(s, pr, PDB_SAVE);
}

static void
process_buyringpage(struct service *s, struct player *pr, const struct UM_RINGPAGEBUY *um) {
    struct chardata* cdata = &pr->data;
    struct ringdata* rdata = &cdata->ringdata;

    if (rdata->npage >= RING_PAGE_MAX) {
        return; // 最大页
    }
    if (cdata->diamond < RING_PAGE_PRICE) {
        return; // 钻石不足
    }
    // do logic
    rdata->npage += 1;
    cdata->diamond -= RING_PAGE_PRICE;
    
    sync_money(s, pr);
    sync_ringpage(s, pr); 
    playerdb_send(s, pr, PDB_SAVE);
    return;
}

static void
process_useringpage(struct service *s, struct player *pr, const struct UM_RINGPAGEUSE *um) {
    //struct ringlogic *self = SERVICE_SELF;

    struct chardata* cdata = &pr->data;
    struct ringdata* rdata = &cdata->ringdata;

    if (um->index == rdata->usepage)
        return;
    if (um->index >= rdata->npage) {
        return;
    }
    // do logic
    rdata->usepage = um->index;

    // refresh attribute
    attrilogic_main(&pr->data);
    
    playerdb_send(s, pr, PDB_SAVE);
    return;
}

static void
login(struct player* pr) {
    struct chardata* cdata = &pr->data;
    struct ringdata* rdata = &cdata->ringdata;
    if (rdata->npage < RING_NPAGE_INIT)
        rdata->npage = RING_NPAGE_INIT;
    else if (rdata->npage > RING_PAGE_MAX)
        rdata->npage = RING_PAGE_MAX;
    if (rdata->usepage >= rdata->npage)
        rdata->usepage = 0;
    if (rdata->nring > RING_MAX)
        rdata->nring = RING_MAX;
}

void
ringlogic_main(struct service *s, struct player *pr, const void *msg, int sz) {
    UM_CAST(UM_BASE, base, msg);
    switch (base->msgid) {
    case IDUM_ENTERHALL:
        login(pr);
        break;
    case IDUM_RINGPAGEBUY: {
        UM_CASTCK(UM_RINGPAGEBUY, buy, base, sz);
        process_buyringpage(s, pr, buy);
        break;
        }
    case IDUM_RINGPAGERENAME: {
        UM_CASTCK(UM_RINGPAGERENAME, rename, base, sz);
        process_renameringpage(s, pr, rename);
        break;
        }
    case IDUM_RINGPAGEUSE: {
        UM_CASTCK(UM_RINGPAGEUSE, use, base, sz);
        process_useringpage(s, pr, use);
        break;
        }
    case IDUM_RINGEQUIP: {
        UM_CASTCK(UM_RINGEQUIP, equip, base, sz);
        process_equipring(s, pr, equip);
        break;
        }
    //case IDUM_RINGSALE: {
        //UM_CASTCK(UM_RINGSALE, sale, base, sz);
        //process_salering(s, pr, sale);
        //break;
        //}
    }
}
