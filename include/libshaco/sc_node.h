#ifndef __sc_node_h__
#define __sc_node_h__

int sc_service_subscribe(const char *name);
int sc_service_publish(const char *name);
int sc_service_send(int source, int dest, const void *msg, int sz);

#ifdef __GNUC__
int sc_service_vsend(int source, int dest, const char *fmt, ...)
__attribute__((format(printf, 3, 4)))
#endif
;

static inline int
sc_handleid(int nodeid, int serviceid) {
    return ((nodeid & 0xff) << 8) | (serviceid & 0xff);
}

static inline int
sc_serviceid_from_handle(int handle) {
    return handle & 0x00ff;
}

static inline int
sc_nodeid_from_handle(int handle) {
    return (handle >> 8) & 0x00ff;
}

#endif
