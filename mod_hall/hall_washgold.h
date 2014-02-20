#ifndef __hall_washgold_h__
#define __hall_washgold_h__

struct module;
struct player;

void hall_washgold_main(struct module *s, struct player *pr, const void *msg, int sz); 
void hall_washgold_time(struct module *s);

#endif
