#include "sh.h"
#include "room_item.h"
#include "room_tplt.h"
#include "room.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "luck_random.h"

static const struct item_tplt *
item_rand(struct room *self, struct room_game *ro, int mode, 
        struct room_item *items, struct player *m) {
    uint32_t id = 0; 
    assert(mode >= 0 && mode < MODE_MAX);
    int lucky = m->detail.attri.lucky;
    int rand = luck_random(&self->randseed, lucky, 0.4f, items->luck_up, &m->luck_factor);
    int i;
    for (i=0; i<items->n; ++i) {
        if (rand <= items->p[i].luck) {
            id = items->p[i].id;
            break;
        }
    }
    if (id > 0) {
        sh_trace("Room %u rand mode item %u", ro->id, id);
        return room_tplt_find_item(self, id);
    } else {
        sh_trace("Room %u rand mode item fail", ro->id);
        return NULL;
    }
}
 
const struct item_tplt *
room_item_rand(struct room *self, struct room_game *ro, struct player *m, 
        const struct item_tplt *item) {
    int8_t mode = room_game_mode(ro);
    if (item->subtype == 0) {
        return item_rand(self, ro, mode, &ro->mode_items[mode], m);
    } else {
        switch (mode) {
        case MODE_FREE:
            if (item->target != ITEM_TARGET_SELF) {
                return item_rand(self, ro, mode, &ro->mode_items[mode], m);
            }
            break;
        case MODE_CO:
            if (item->target != ITEM_TARGET_SELF &&
                item->target != ITEM_TARGET_FRIEND) {
                return item_rand(self, ro, mode, &ro->mode_items[mode], m);
            }
            break;
        case MODE_FIGHT:
            if (item->target != ITEM_TARGET_SELF &&
                item->target != ITEM_TARGET_ENEMY) {
                struct player *front = room_member_front(ro, m);
                if (front && ((front->depth - m->depth) >= 20))
                    return item_rand(self, ro, mode, &ro->fight_items2, m);
                else
                    return item_rand(self, ro, mode, &ro->mode_items[mode], m);
            }
            break;
        }
    }
    return item;
}

static int
item_cmp(const void *p1, const void *p2) {
    const struct luck_item* i1 = p1;
    const struct luck_item* i2 = p2;
    return i2->luck - i1->luck;
}

static void
item_init(struct room_item *items, struct room *self, const struct map_tplt *map, 
        const uint32_t *itemids, uint16_t nitemid) {
    items->n = 0;
    int i;
    for (i=0; i<nitemid; ++i) {
        uint32_t itemid = itemids[i];
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
}

void
room_item_init(struct room *self, struct room_game *ro, const struct map_tplt *map) {
    item_init(&ro->mode_items[MODE_FREE], self, map, map->freeitem, map->nfreeitem);
    item_init(&ro->mode_items[MODE_CO],   self, map, map->coitem, map->ncoitem);
    item_init(&ro->mode_items[MODE_FIGHT],self, map, map->fightitem, map->nfightitem);
    item_init(&ro->fight_items2, self, map, map->fightitem2, map->nfightitem2);
}

void
room_item_fini(struct room_game *ro) {
}
