#include "player.h"
#include "freeid.h"
#include "hashid.h"
#include <string.h>

struct player_holder {
    int cmax;
    int hmax;
    int gmax;
    struct freeid fi;
    struct hashid hi;
    struct player* p;
};

static struct player_holder* PH = NULL;

void
_allocplayers(int cmax, int hmax, int gmax) {
    PH = malloc(sizeof(*PH));
    if (cmax < 0)
        cmax = 1;
    if (hmax < cmax)
        hmax = cmax;
    if (gmax < 0)
        gmax = 1;
    PH->cmax = cmax;
    PH->hmax = hmax;
    PH->gmax = gmax;
    PH->p = malloc(sizeof(struct player) * gmax*cmax);
    memset(PH->p, 0, sizeof(struct player) * gmax*cmax);
    freeid_init(&PH->fi, gmax*cmax, gmax*hmax);
    hashid_init(&PH->hi, gmax*cmax, gmax*hmax);
}
void
_freeplayers() {
    if (PH) {
        freeid_fini(&PH->fi);
        hashid_fini(&PH->hi);
        PH = NULL;
    }
}

#define _isvalidid(gid, cid) \
    ((gid) >= 0 && (gid) < PH->gmax && \
     (cid) >= 0 && (cid) < PH->hmax)
#define _hashid(gid, cid) \
    ((gid)*PH->hmax+(cid))

struct player*
_getplayer(uint16_t gid, int cid) {
    if (_isvalidid(gid, cid)) {
        int id = freeid_find(&PH->fi, _hashid(gid, cid));
        if (id >= 0) {
            return &PH->p[id];
        }
    }
    return NULL;
}
struct player*
_getplayerbyid(uint32_t charid) {
    if (charid == 0) {
        return NULL;
    }
    int id = hashid_find(&PH->hi, charid);
    if (id >= 0) {
        return &PH->p[id];
    }
    return NULL;
}
struct player*
_allocplayer(uint16_t gid, int cid) {
    struct player* p;
    if (_isvalidid(gid, cid)) {
        int id = freeid_alloc(&PH->fi, _hashid(gid, cid));
        if (id >= 0) {
            p = &PH->p[id];
            p->gid = gid;
            p->cid = cid;
            return p;
        }
    }
    return NULL;
}
int 
_hashplayer(struct player* p) {
    int id = hashid_alloc(&PH->hi, p->data.charid);
    if (id == -1)
        return 1;
    assert(id == p-PH->p);
    return 0;
}
void
_freeplayer(struct player* p) {
    assert(p->status != PS_FREE);
    int hashid = _hashid(p->gid, p->cid);
    int id1 = freeid_free(&PH->fi, hashid);
    assert(id1 >= 0);
    assert(id1 == p-PH->p);
    if (p->data.charid > 0) {
        int id2 = hashid_free(&PH->hi, p->data.charid);
        assert(id1 == id2);
        p->data.charid = 0;
        p->data.name[0] = '\0';
    }
    p->status = PS_FREE;
    p->gid = 0;
    p->cid = 0;
}
