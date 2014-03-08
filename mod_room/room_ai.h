#ifndef __room_ai_h__
#define __room_ai_h__

struct module;
struct room_game;
struct player;

void ai_init(struct player *m, int level);
void ai_fini(struct player *m);
void ai_main(struct module *s, struct room_game *ro, struct player *pr);

#endif
