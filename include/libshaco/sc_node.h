#ifndef __sc_node_h__
#define __sc_node_h__

#include "sc_service.h"

//#define MT_SYS  0
#define MT_TEXT 1
#define MT_UM   2
#define MT_MONITOR 3
#define MT_LOG 4
#define MT_USR1 5
#define MT_USR2 6
#define MT_USR3 7

// publish flag
#define PUB_SER 1
#define PUB_MOD 2
#define PUB_BOTH PUB_SER|PUB_MOD

struct sh_node_addr {
    char naddr[40];
    char gaddr[40];
    int  nport;
    int  gport;
};

int sc_service_start(const char *name, int handle, const struct sh_node_addr *addr);
int sc_service_exit(int handle);

int sc_service_subscribe(const char *name);
int sc_service_publish(const char *name, int flag);
int sh_service_send(int source, int dest, int type, const void *msg, int sz);
int sh_service_broadcast(int source, int dest, int type, const void *msg, int sz);

#ifdef __GNUC__
int sc_service_vsend(int source, int dest, const char *fmt, ...)
__attribute__((format(printf, 3, 4)))
#endif
;

bool sc_service_has(int vhandle, int handle);

int sc_service_minload(int vhandle);
int sc_service_nextload(int vhandle);

int sh_handler(const char *name, int *handle);
int sh_handle_publish(const char *name, int flag);

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
