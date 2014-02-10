#ifndef __hall_award_h__
#define __hall_award_h__

struct service;
struct player;

void hall_award_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
