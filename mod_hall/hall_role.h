#ifndef __hall_role_h__
#define __hall_role_h__

struct module;
struct player;

void hall_role_main(struct module *s, struct player *pr, const void *msg, int sz); 
void hall_role_time(struct module *s);

#endif
