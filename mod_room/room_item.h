#ifndef __room_item_h__
#define __room_item_h__

#include <stdint.h>

struct room;
struct player;
struct room_game;
struct map_tplt;
struct item_tplt;

void room_item_init(struct room *self, struct room_game *ro, const struct map_tplt *map);
void room_item_fini(struct room_game *ro);

const struct item_tplt * 
room_item_rand(struct room *self, 
        struct room_game *ro, 
        struct player *m, 
        const struct item_tplt *item);

#endif
