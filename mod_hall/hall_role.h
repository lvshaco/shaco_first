#ifndef __hall_role_h__
#define __hall_role_h__

struct service;
struct player;

void hall_role_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
