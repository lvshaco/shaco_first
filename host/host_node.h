#ifndef __host_node_h__
#define __host_node_h__

#include <stdint.h>

struct host_node {
    uint16_t tid;   // = array index
    uint16_t sid;   // = array index + 1
    uint32_t addr;
    uint16_t port;
    int connid;
};

#define HOST_NODE_SUBID_MAX 1024

int host_node_init();
void host_node_free();

int host_node_typeid(const char* name);
const char* host_node_typename(int id);

int host_node_register_types(const char* types[], int n);
int host_node_register(struct host_node* node);
int host_node_unregister(uint16_t tid, uint16_t sid);
int host_node_disconnect(int connid);

#endif
