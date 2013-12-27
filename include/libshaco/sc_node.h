#ifndef __sc_node_h__
#define __sc_node_h__

#include <stdint.h>
#include <stdbool.h>

#define NODE_MAX 256
#define SUB_MAX 10

struct sc_node {
    int connid;
    uint32_t naddr;
    uint16_t nport;
    uint32_t gaddr;
    uint16_t gport;
};

struct _remote {
    struct sc_Nnode[NODE_MAX];
};

int sc_service_subscribe(int id, const char *name);
int sc_service_publish(const char *name);
int sc_service_send(int handle);

int sc_Nnode_register(int nodeid, struct sc_Nnode* node);
int sc_Nnode_unregister(int nodeid);
int sc_Nnode_disconnect(int connid);
int sc_Nnode_send(int nodeid, void *msg, int sz);

#endif
