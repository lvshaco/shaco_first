#include "sc_gate.h"
#include "sc.h"
#include "sc_init.h"
#include "sc_net.h"
#include "sc_timer.h"
#include "sc_service.h"
#include "sc_log.h"
#include "sc_env.h"
#include "sc.h"
#include "freeid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct gate {
    int serviceid;
    int cmax;
    int used;
    struct freeid fi;
    struct gate_client* p;
};

static struct gate* G = NULL;

int
sc_gate_prepare(int cmax, int hmax) {
    if (cmax < 0)
        cmax = 1;
    G->cmax = cmax;
    struct gate_client* c = malloc(sizeof(struct gate_client) * cmax);
    memset(c, 0, sizeof(struct gate_client) * cmax);
    G->p = c;
    freeid_init(&G->fi, cmax, hmax);
    return 0;
}

static void
_notify_gate_event(int event) {
    service_main(G->serviceid, event, 0, NULL, 0);
}

struct gate_client*
sc_gate_acceptclient(int connid) {
    assert(connid != -1);
    int id = freeid_alloc(&G->fi, connid);
    if (id == -1) {
        sc_net_close_socket(connid, true);
        return NULL;
    }
    assert(id >= 0 && id < G->cmax);
    struct gate_client* c = &G->p[id];
    assert(c->status == GATE_CLIENT_FREE);
    c->connid = connid;
    c->status = GATE_CLIENT_CONNECTED;
    c->active_time = sc_timer_now();
    sc_net_subscribe(connid, true);
    G->used++;
    _notify_gate_event(GATE_EVENT_ONACCEPT); 
    return c;
}

void 
sc_gate_loginclient(struct gate_client* c) { 
    if (c->status == GATE_CLIENT_CONNECTED) {
        c->status = GATE_CLIENT_LOGINED; 
        c->active_time = sc_timer_now();
    }
}

bool
sc_gate_disconnclient(struct gate_client* c, bool force) {
    if (c->status == GATE_CLIENT_FREE)
        return true;
    bool closed = sc_net_close_socket(c->connid, force);
    if (closed) {
        int id = freeid_free(&G->fi, c->connid);
        assert(id == (c-G->p));
        c->status = GATE_CLIENT_FREE;
        c->active_time = 0;
        G->used--;
        _notify_gate_event(GATE_EVENT_ONDISCONN); 
    } else {
        if (c->status != GATE_CLIENT_LOGOUTED) {
            c->status = GATE_CLIENT_LOGOUTED;
            c->active_time = sc_timer_now();
        }
    }
    return closed;
}

struct gate_client* 
sc_gate_getclient(int connid) {
   int id = freeid_find(&G->fi, connid);
   if (id == -1)
       return NULL;
   assert(id >= 0 && id < G->cmax);
   struct gate_client* c = &G->p[id];
   assert(c->connid == connid);
   return c;
}

struct gate_client*
sc_gate_firstclient() {
    return G->p;
}
int
sc_gate_maxclient() {
    return G->cmax;
}
int 
sc_gate_usedclient() {
    return G->used;
}
int 
sc_gate_clientid(struct gate_client* c) {
    return c-G->p;
}

static void
sc_gate_init() {
    G = malloc(sizeof(*G));
    memset(G, 0, sizeof(*G));
    G->serviceid = service_query_id("gate"); 
    if (G->serviceid != SERVICE_INVALID) {
        if (service_prepare("gate")) {
            sc_exit("gate init fail");
        }
    }
}

static void 
sc_gate_fini() {
    if (G == NULL)
        return;
    freeid_fini(&G->fi);
    free(G->p);
    G->p = NULL;
    free(G);
    G = NULL;
}

SC_LIBRARY_INIT_PRIO(sc_gate_init, sc_gate_fini, 25)
