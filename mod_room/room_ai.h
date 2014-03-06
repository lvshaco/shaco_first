#ifndef __room_ai_h__
#define __room_ai_h__

struct module;
struct gameroom;
struct player;

void ai_init(struct player *m, int level);
void ai_fini(struct player *m);
void ai_main(struct module *s, struct gameroom *ro, struct player *pr);

#endif
