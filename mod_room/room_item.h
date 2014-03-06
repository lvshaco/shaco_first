#ifndef __room_item_h__
#define __room_item_h__

#include <stdint.h>

struct room;
struct player;
struct gameroom;
struct map_tplt;

int room_item_init(struct room *self, struct gameroom *ro, const struct map_tplt *map);
void room_item_fini(struct gameroom *ro);
uint32_t room_item_rand(struct room *self, struct gameroom *ro, struct player *m);

#endif
