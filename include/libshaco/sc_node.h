#ifndef __sc_node_h__
#define __sc_node_h__

#include <stdint.h>
#include <stdbool.h>

int sc_service_subscribe(const char *name);
int sc_service_publish(const char *name);
int sc_service_send(int source, int dest, const void *msg, int sz);
int sc_service_vsend(int source, int dest, const char *fmt, ...);

static inline int
sc_handle_id(int nodeid, int serviceid) {
    return ((nodeid & 0xff) << 8) | (serviceid & 0xff)
}

static inline int
sc_service_id_from_handle(int handle) {
    return handle & 0x00ff;
}

static inline int
sc_node_id_from_handle(int handle) {
    return (handle >> 8) & 0x00ff;
}

#endif
