#include "host_gate.h"
#include "host_net.h"
#include "freeid.h"
#include <stdlib.h>
#include <string.h>

struct gate {
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
    int i;
    for (i=0; i<cmax; ++i) {
        c[i].connid = -1;
        c[i].active_time = 0;
    }
    G->p = c;
    freeid_init(&G->fi, cmax, hmax);
    return 0;
}

struct gate_client*
host_gate_acceptclient(int connid, uint64_t now) {
    assert(connid != -1);
    int id = freeid_alloc(&G->fi, connid);
    if (id == -1) {
        host_net_close_socket(connid, true);
        return NULL;
    }
    assert(id >= 0 && id < G->cmax);
    struct gate_client* c = &G->p[id];
    assert(c->connid == -1);
    c->connid = connid;
    c->active_time = now;
    host_net_subscribe(connid, true);
    G->used++;
    return c;
}

bool
host_gate_disconnclient(struct gate_client* c, bool force) {
    if (c->connid == -1)
        return true;
    bool closed = host_net_close_socket(c->connid, force);
    if (closed) {
        int id = freeid_free(&G->fi, c->connid);
        assert(id == (c-G->p));
        c->connid = -1;
        c->active_time = 0;
        G->used--;
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
