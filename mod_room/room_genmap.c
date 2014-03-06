#include "room_genmap.h"
#include "room_map.h"
#include "tplt_struct.h"
#include <stdint.h>
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

static inline int
_randhit(int base, int rate) {
    if (rate < base)
        return _rand() % base < rate;
    else
        return 1;
}

static inline uint32_t
_spectex(const struct map_tplt* tplt, int type) {
    int index = type - CELL_SPEC;
    if (index >= 0 && index < tplt->nspectex) {
        return tplt->spectex[index];
    } else {
        return 0;
    }
}

static inline uint32_t
_colortex(const struct map_tplt* tplt, int index) {
    return (index >= 0 && index < tplt->ncolortex) ? tplt->colortex[index] : 0;
}

static uint32_t
_randcell(const struct map_tplt* tplt, struct roommap* m, uint16_t h) {
    int type, texid;
    if (_randhit(10000, h-1)) {
        type = CELL_SHI;
        texid  = _spectex(tplt, type);
    } else {
        int index = (h-1)/100;
        struct roommap_typeidlist tilist = roommap_gettypeidlist(m, index);
        if (tilist.first && tilist.num > 0)
            type = tilist.first[_rand() % tilist.num].id;
        else
            type = 0;
        texid = _colortex(tplt, index);
    }
    return 1000 + type*100 + texid;
}

static void
_gencell(const struct map_tplt* tplt, struct roommap* m, uint16_t h, 
         struct roommap_cell* in, struct genmap_cell* out) {
    out->block = in->block;
    if (in->isassign) {
        if (in->cellrate == 0) {
            out->cellid = 0;
        } else if (_randhit(100, in->cellrate)) {
            out->cellid = in->cellid;
        } else {
            out->cellid = _randcell(tplt, m, h);
        }
        if (in->cellid == 0) {
            if (_randhit(100, in->itemrate)) {
                out->itemid = in->itemid;
            } else {
                out->itemid = 0;
                out->cellid = _randcell(tplt, m, h);
            }
        } else {
            if (in->itemrate == 0) {
                out->itemid = 0;
            } else if (_randhit(100, in->itemrate)) {
                out->itemid = in->itemid;
            } else {
                out->itemid = 300501; // 机关盒
            }
        }
    } else {
        out->cellid = _randcell(tplt, m, h);
        out->itemid = 0;
    }
}

struct genmap* 
genmap_create(const struct map_tplt* tplt, struct roommap* m, uint32_t randseed) {
    _srand(randseed);

    uint16_t w = tplt->width;
    uint16_t h = tplt->height;

    if (m->header.height < h || m->header.width < w)
        return NULL;

    struct genmap* self = (struct genmap*)malloc(sizeof(*self) + 
            sizeof(struct genmap_cell) * w*h);
    self->width = w;
    self->height = h;

    struct genmap_cell* pout = self->cells;
    struct roommap_cell* pin = ROOMMAP_CELL_ENTRY(m);
    uint32_t i;
    for (i=0; i<h*w; ++i) {
        _gencell(tplt, m, i/w+1, &pin[i], &pout[i]);
    }
    return self;
}

void 
genmap_free(struct genmap* self) {
    free(self);
}
