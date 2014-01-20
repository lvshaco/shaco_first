#ifndef __playlogic_h__
#define __Playlogic_h__

struct service;
struct player;

void playlogic_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
