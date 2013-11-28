#ifndef __genmap_h__
#define __genmap_h__

#include <stdint.h>

struct genmap_cell {
    uint32_t cellid;
    uint32_t itemid;
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

struct map_tplt;
struct roommap;

struct genmap* genmap_create(struct map_tplt* tplt, struct roommap* m, uint32_t randseed);
void genmap_free(struct genmap* self);

#endif
