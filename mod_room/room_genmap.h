#ifndef __room_genmap_h__
#define __room_genmap_h__

#include <stdint.h>

struct genmap_cell {
    uint32_t cellid;
    uint32_t itemid;
    uint16_t block;
};

struct genmap {
    uint16_t width;
    uint16_t height;
    struct genmap_cell cells[];
};

#define CELL_SPEC 7 // 特殊块
#define CELL_TU  7  // 土块
#define CELL_SHI 8  // 石块
#define CELL_YAN 9  // 岩块

#define CELL_IS_SHI(cellid) (((cellid)%1000)/100 == 8)

struct map_tplt;
struct roommap;

struct genmap* genmap_create(const struct map_tplt* tplt, struct roommap* m, uint32_t randseed);
void genmap_free(struct genmap* self);

#define GENMAP_CELL(m, w, h) (&((m)->cells[h*(m)->width +w]))

#endif
