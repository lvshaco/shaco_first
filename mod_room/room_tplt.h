#ifndef __room_tplt_h__
#define __room_tplt_h__

#include "room.h"
#include "tplt_include.h"
#include "tplt_struct.h"

struct module;
struct room;

int room_tplt_init(struct room* self);
void room_tplt_fini(struct room* self);
int room_tplt_main(struct module *s);

// helper function
static inline struct item_tplt*
room_tplt_find_item(struct room *self, uint32_t itemid) {
    const struct tplt_visitor* visitor = tplt_get_visitor(self->T, TPLT_ITEM);
    if (visitor) {
        return tplt_visitor_find(visitor, itemid);
    }
    return NULL;
}

static inline const struct map_tplt*
room_tplt_find_map(struct room *self, uint32_t mapid) {
    const struct tplt_visitor* visitor = tplt_get_visitor(self->T, TPLT_MAP);
    if (visitor) 
        return tplt_visitor_find(visitor, mapid);
    return NULL; 
}

static inline struct roommap *
room_mapdata_find(struct room *self, uint32_t id) {
    if (self->MH)
        return sh_hash_find(self->MH, id);
    return NULL;
}

#endif
