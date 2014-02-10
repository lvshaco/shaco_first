#ifndef __hall_play_h__
#define __hall_Play_h__

struct service;
struct player;

void hall_play_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
