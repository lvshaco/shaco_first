#ifndef __host_node_h__
#define __host_node_h__

#include <stdint.h>
#include <stdbool.h>

struct host_node {
    uint16_t id; // see HNODE_ID
    uint32_t addr;
    uint16_t port;
    int connid;
};

#define HNODE_SID_MAX 0x3ff
#define HNODE_TID_MAX 0x3f
#define HNODE_NAME_MAX 16

#define HNODE_ID(tid, sid) ((((uint16_t)(tid)&0x3f) << 10) | ((uint16_t)(sid)&0x3ff))
#define HNODE_TID(id) (((uint16_t)(id) >> 10) & 0x3f)
#define HNODE_SID(id) ((uint16_t)(id)&0x3ff)

// me node
uint16_t host_id();
struct host_node* host_me();
int host_register_me(struct host_node* me);

int  host_node_typeid(const char* name);
const char* host_node_typename(uint16_t tid);
struct host_node* host_node_get(uint16_t id);

int  host_node_init();
void host_node_free();
int  host_node_register_types(const char* types[], int n);
bool host_node_is_register(uint16_t id);
int  host_node_register(struct host_node* node);
int  host_node_unregister(uint16_t id);
int  host_node_disconnect(int connid);
void host_node_foreach(uint16_t tid, int (*cb)(struct host_node*, void* ud), void* ud);

#endif
