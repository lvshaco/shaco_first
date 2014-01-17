#ifndef __role_logic_h__
#define __role_logic_h__

struct service;
struct player;

void rolelogic_main(struct service *s, struct player *pr, const void *msg, int sz); 

#endif
