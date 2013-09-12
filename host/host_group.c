#include "host_group.h"
#include "host_net.h"
#include "host_log.h"
#include <stdlib.h>
#include <assert.h>

struct _slot {
    int id;
    int used;
};

struct host_group {
    int nslot;
    struct _slot* slots;
    struct _slot* free_slot;
};

static void
_init_slots(struct _slot* s, int begin, int end) {
    int i;
    for (i=begin; i<end-1; ++i) {
        s[i].id = i+1;
        s[i].used = 0;
    }
    s[i].id = -1;
}

static struct _slot*
_alloc_slot(struct host_group* g) {
    struct _slot* free = g->free_slot;
    if (free) {
        if (free->id >= 0) {
            g->free_slot = &g->slots[free->id];
        } else {
            g->free_slot = NULL;
        }
        free->used = 1;
        return free;
    } else {
        assert(g->nslot > 0);
        int nslot_old = g->nslot;
        int cap = 1;
        while (cap < nslot_old)
            cap *= 2;
        g->nslot = cap;
        g->slots = realloc(g->slots, sizeof(struct _slot) * g->nslot);
        _init_slots(g->slots, nslot_old, g->nslot);
        g->free_slot = &g->slots[nslot_old];
        return _alloc_slot(g);
    }
}

static void
_free_slot(struct host_group* g, struct _slot* s) {
    int id = s - g->slots;
    assert(id >= 0);
    if (g->free_slot) {
        s->id = g->free_slot - g->slots;
    } else {
        s->id = -1;
    }
    s->used = 0;
    g->free_slot = s;
}

struct host_group*
host_group_create(int init) {
    if (init <= 0)
        init = 1;
    struct host_group* g = malloc(sizeof(*g));
    g->nslot = init;
    g->slots = malloc(sizeof(struct _slot) * init);
    g->free_slot = g->slots;
    _init_slots(g->slots, 0, init);
    return g;
}

void
host_group_free(struct host_group* g) {
    if (g == NULL)
        return;
    free(g->slots);
    free(g);
}

int
host_group_join(struct host_group* g, int connection) {
    struct _slot* s = _alloc_slot(g);
    if (s == NULL)
        return -1;
    s->id = connection;
    return s - g->slots;
}

int
host_group_disjoin(struct host_group* g, int slot, int connection) {
    if (slot < 0 || slot >= g->nslot) {
        return -1;
    }
    struct _slot* s = &g->slots[slot];
    if (s->id == connection) {
        _free_slot(g, s);
        return 0;
    } else {
        host_error("host group dismatch slot%d connection%d(%d expected)", 
                slot, connection, s->id);
        return -1;
    }
}

int
host_group_broadcast(struct host_group* g, void* msg, int sz) {
    struct _slot* s = NULL;
    int n = 0;
    int i;
    for (i=0; i<g->nslot; ++i) {
        s = &g->slots[i];
        if (s->used) {
            host_net_send(s->id, msg, sz);
            n++;
        }
    }
    return n;
}
