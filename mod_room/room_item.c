#include "sh.h"
#include "room_item.h"
#include "room_tplt.h"
#include "room.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "luck_random.h"

uint32_t 
room_item_rand(struct room *self, struct gameroom *ro, struct player *m) {
    struct room_item *items = &ro->items;
    int lucky = m->detail.attri.lucky;
    int rand = luck_random(self->randseed, lucky, 0.4f, items->luck_up, &m->luck_factor);
    int i;
    for (i=0; i<items->n; ++i) {
        if (rand <= items->p[i].luck) {
            return items->p[i].id;
        }
    }
    return 0;
}

static int
item_cmp(const void *p1, const void *p2) {
    const struct luck_item* i1 = p1;
    const struct luck_item* i2 = p2;
    return i2->luck - i1->luck;
}

int
room_item_init(struct room *self, struct gameroom *ro, const struct map_tplt *map) {
    struct room_item *items = &ro->items;
    items->n = 0;
    int i;
    for (i=0; i<map->nfightitem; ++i) {
        uint32_t itemid = map->fightitem[i];
        struct item_tplt *itemt = room_tplt_find_item(self, itemid);
        if (itemt) {
            items->p[items->n].luck = itemt->luck;
            items->p[items->n].id = itemt->id;
            items->n++;
        } else {
            sh_error("can not found item %u, in map %u", itemid, map->id);
        }
    }
    qsort(items->p, items->n, sizeof(items->p[0]), item_cmp);
    uint32_t cur_luck = 0;
    for (i=0; i<items->n; ++i) {
        items->p[i].luck += cur_luck;
        cur_luck = items->p[i].luck;
    }
    items->luck_up = cur_luck;
    return 0;
}

void
room_item_fini(struct gameroom *ro) {
}
