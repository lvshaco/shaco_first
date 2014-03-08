#ifndef __room_pull_h__
#define __room_pull_h__

struct module;
struct room_game;

void room_pull_init(struct module *s, struct room_game *ro);
void room_pull_update(struct module *s, struct room_game *ro);
void room_pull_on_join(struct module *s, struct room_game *ro);
void room_pull_on_leave(struct module *s, struct room_game *ro);

#endif
