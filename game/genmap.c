#include "genmap.h"
#include "roommap.h"
#include "tplt_struct.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

static uint64_t next = 1;

static inline int 
_rand(void) {
    next = next * 1103515245 + 12345;
    return((uint32_t)(next/65536) % 32768);
}

static inline void 
_srand(uint32_t seed) {
    next = seed;
}

static inline bool
_randhit(int base, int rate) {
    if (rate > 0)
        return _rand() % base < rate;
    else
        return true;
}

static inline uint32_t
_spectex(struct map_tplt* tplt, int typeid) {
    int index = typeid - CELL_SPEC;
    if (index >= 0 && index < tplt->nspectex) {
        return tplt->spectex[index];
    } else {
        return 0;
    }
}

static inline uint32_t
_colortex(struct map_tplt* tplt, int index) {
    return (index >= 0 && index < tplt->ncolortex) ? tplt->colortex[index] : 0;
}

static uint32_t
_randcell(struct map_tplt* tplt, struct roommap* m, int height) {
    int typeid, texid;
    int index = height/100;
    if (_randhit(10000, height)) {
        typeid = CELL_SHI;
        texid  = _spectex(tplt, typeid);
    } else {
        struct roommap_typeidlist tilist = roommap_typeidlist(m, index);
        typeid = tilist.num > 0 ? tilist.first[_rand() % tilist.num].id : 0;
        texid = _colortex(tplt, index);
    }
    return 1000 + typeid*100 + texid;
}

static void
_gencell(struct map_tplt* tplt, struct roommap* m, int height, 
         struct roommap_cell* in, struct genmap_cell* out) {
    if (in->isassign) {
        if (_randhit(100, in->cellrate)) {
            out->cellid = in->cellid; 
        } else {
            out->cellid = _randcell(tplt, m, height);
        }
        if (in->cellid == 0) {
            if (_randhit(100, in->itemrate)) {
                out->itemid = in->itemid;
            } else {
                out->itemid = 0;
                out->cellid = _randcell(tplt, m, height);
            }
        } else {
            if (_randhit(100, in->itemrate)) {
                out->itemid = in->itemid;
            } else {
                out->itemid = 300501; // 机关盒
            }
        }
    } else {
        out->cellid = _randcell(tplt, m, height);
        out->itemid = 0;
    }
}

struct genmap* 
genmap_create(struct map_tplt* tplt, struct roommap* m, uint32_t randseed) {
    _srand(randseed);

    uint16_t w = tplt->width;
    uint16_t h = tplt->height;

    if (m->header.row < h || m->header.col < w)
        return NULL;

    struct genmap* self = (struct genmap*)malloc(sizeof(*self) + 
            sizeof(struct genmap_cell) * w*h);
    self->width = w;
    self->height = h;

    struct genmap_cell* pout = self->cells;
    struct roommap_cell* pin = ROOMMAP_CELL_ENTRY(m);
    uint32_t i;
    for (i=0; i<h*w; ++i) {
        _gencell(tplt, m, i/h+1, &pin[i], &pout[i]);
    }
    return self;
}

void 
genmap_free(struct genmap* self) {
    free(self);
}
