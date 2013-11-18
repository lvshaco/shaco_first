#ifndef __sc_node_h__
#define __sc_node_h__

#include <stdint.h>
#include <stdbool.h>

struct sc_node {
    union {
        struct {
            uint16_t sid:10;
            uint16_t tid:6;
        };
        uint16_t id; // see HNODE_ID
    };
    uint32_t addr;
    uint16_t port;
    uint32_t gaddr;
    uint16_t gport;
    int connid;
    int load;
};

#define HNODE_SID_MAX 0x3ff
#define HNODE_TID_MAX 0x3f
#define HNODE_NAME_MAX 16
#define HNODESTR_MAX 128

#define HNODE_ID(tid, sid) ((((uint16_t)(tid)&0x3f) << 10) | ((uint16_t)(sid)&0x3ff))
#define HNODE_TID(id) (((uint16_t)(id) >> 10) & 0x3f)
#define HNODE_SID(id) ((uint16_t)(id)&0x3ff)
#define HNODE_MAX HNODE_ID(HNODE_TID_MAX, HNODE_SID_MAX)

// me node
uint16_t sc_id();
struct sc_node* sc_me();
int sc_register_me(struct sc_node* me);

int  sc_node_typeid(const char* name);
const char* sc_node_typename(uint16_t tid);
const struct sc_node* sc_node_get(uint16_t id);

int  sc_node_types();
int  sc_node_register_types(const char* types[], int n);
bool sc_node_is_register(uint16_t id);
int  sc_node_register(struct sc_node* node);
int  sc_node_unregister(uint16_t id);
int  sc_node_disconnect(int connid);
void sc_node_foreach(uint16_t tid, int (*cb)(const struct sc_node*, void* ud), void* ud);
const char* sc_strnode(const struct sc_node* node, char str[HNODESTR_MAX]);

// load
const struct sc_node* sc_node_minload(uint16_t tid);
void sc_node_updateload(uint16_t id, int value);
void sc_node_setload(uint16_t id, int value);

#endif
