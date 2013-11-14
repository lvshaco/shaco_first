#include "host_gate.h"
#include "host_net.h"
#include "host_timer.h"
#include "host_service.h"
#include "host_log.h"
#include "host.h"
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
host_gate_init() {
    G = malloc(sizeof(*G));
    memset(G, 0, sizeof(*G));
    G->serviceid = -1;

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "gate,%s", host_getstr("gate_handler", ""));
    if (service_load(tmp) == 0) {
        G->serviceid = service_query_id("gate");
    } 
    return 0;
}

void 
host_gate_fini() {
    if (G == NULL)
        return;
    freeid_fini(&G->fi);
    free(G->p);
    G->p = NULL;
    free(G);
    G = NULL;
}

int
host_gate_prepare(int cmax, int hmax) {
    if (cmax < 0)
        cmax = 1;
    G->cmax = cmax;
    struct gate_client* c = malloc(sizeof(struct gate_client) * cmax);
    memset(c, 0, sizeof(struct gate_client*) * cmax);
    G->p = c;
    freeid_init(&G->fi, cmax, hmax);
    return 0;
}

static void
_notify_gate_event(int event) {
    struct service_message sm;
    sm.sessionid = event; // reuse for GATE_EVENT
    sm.source = SERVICE_HOST;
    sm.sz = 0;
    sm.msg = NULL;
    service_notify_service(G->serviceid, &sm);
}

struct gate_client*
host_gate_acceptclient(int connid) {
    assert(connid != -1);
    int id = freeid_alloc(&G->fi, connid);
    if (id == -1) {
        host_net_close_socket(connid, true);
        return NULL;
    }
    assert(id >= 0 && id < G->cmax);
    struct gate_client* c = &G->p[id];
    assert(c->status == GATE_CLIENT_FREE);
    c->connid = connid;
    c->status = GATE_CLIENT_CONNECTED;
    c->active_time = host_timer_now();
    host_net_subscribe(connid, true);
    G->used++;
    _notify_gate_event(GATE_EVENT_ONACCEPT); 
    return c;
}

void 
host_gate_loginclient(struct gate_client* c) { 
    if (c->status == GATE_CLIENT_CONNECTED) {
        c->status = GATE_CLIENT_LOGINED; 
        c->active_time = host_timer_now();
    }
}

bool
host_gate_disconnclient(struct gate_client* c, bool force) {
    if (c->status == GATE_CLIENT_FREE)
        return true;
    bool closed = host_net_close_socket(c->connid, force);
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
            c->active_time = host_timer_now();
        }
    }
    return closed;
}

struct gate_client* 
host_gate_getclient(int connid) {
   int id = freeid_find(&G->fi, connid);
   if (id == -1)
       return NULL;
   assert(id >= 0 && id < G->cmax);
   struct gate_client* c = &G->p[id];
   assert(c->connid == connid);
   return c;
}

struct gate_client*
host_gate_firstclient() {
    return G->p;
}
int
host_gate_maxclient() {
    return G->cmax;
}
int 
host_gate_usedclient() {
    return G->used;
}
int 
host_gate_clientid(struct gate_client* c) {
    return c-G->p;
}
