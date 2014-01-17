#ifndef __ringlogic_h__
#define __ringlogic_h__

struct service;
struct player;

void ringlogic_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
