#ifndef __host_group_h__
#define __host_group_h__

struct host_group;
struct host_group* host_group_create(int init);
void host_group_free(struct host_group* g);
int host_group_join(struct host_group* g, int connection);
int host_group_disjoin(struct host_group* g, int slot, int connection); 
int host_group_broadcast(struct host_group* g, void* msg, int sz);

#endif
