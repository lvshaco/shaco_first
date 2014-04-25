#ifndef __room_genmap_h__
#define __room_genmap_h__

#include <stdint.h>

struct genmap_cell {
    uint32_t cellid;
    uint32_t itemid;
    uint32_t block;
};

struct genmap {
    uint32_t depth;
    uint16_t width;
    uint16_t height;
    uint8_t *ntypes;
    struct genmap_cell *cells;
};

#define CELL_SPEC 7 // 特殊块
#define CELL_TU  7  // 土块
#define CELL_SHI 8  // 石块
#define CELL_YAN 9  // 岩块

#define CELL_TYPE(cellid) (((cellid)%1000)/100)
#define CELL_IS_SHI(cellid) ((CELL_TYPE(cellid) == 8) || (CELL_TYPE(cellid) == 9))

struct map_tplt;
struct roommap;

struct genmap* genmap_create(const struct map_tplt* tplt, struct roommap* m, uint32_t randseed);
void genmap_free(struct genmap* self);

#define GENMAP_CELL(m, w, h) (&((m)->cells[h*(m)->width +w]))
#define MAP_DEPTH(h) (((h)-1)/100)
#define MAP_NTYPE(m, d) (((d) >= 0 && (d) < (m)->depth) ? (m)->ntypes[d] : 0)

#endif
