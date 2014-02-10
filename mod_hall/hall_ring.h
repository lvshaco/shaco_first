#ifndef __hall_ring_h__
#define __hall_ring_h__

struct service;
struct player;

void hall_ring_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
