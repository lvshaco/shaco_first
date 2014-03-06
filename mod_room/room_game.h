#ifndef __room_game_h__
#define __room_game_h__

struct module;
struct player;
struct room;

int  game_init(struct room *self);
void game_fini(struct room *self);
void game_player_main(struct module *s, struct player *m, const void *msg, int sz);
void game_main(struct module *s, int source, const void *msg, int sz);
void game_time(struct module* s);

#endif
