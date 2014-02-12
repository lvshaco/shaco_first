#ifndef __sh_node_h__
#define __sh_node_h__

#include "sh_module.h"

//#define MT_SYS  0
#define MT_TEXT 1
#define MT_UM   2
#define MT_MONITOR 3
#define MT_LOG 4
#define MT_POINTER 5

// publish flag
#define PUB_SER 1
#define PUB_MOD 2
#define PUB_BOTH PUB_SER|PUB_MOD

// subscribe flag
#define SUB_LOCAL  1
#define SUB_REMOTE 2

struct sh_node_addr {
    char naddr[40];
    char gaddr[40];
    int  nport;
    int  gport;
};

int sh_module_start(const char *name, int handle, const struct sh_node_addr *addr);
int sh_module_exit(int handle);

int sh_module_subscribe(const char *name, int flag);
int sh_module_publish(const char *name, int flag);
int sh_module_send(int source, int dest, int type, const void *msg, int sz);
int sh_module_broadcast(int source, int dest, int type, const void *msg, int sz);

#ifdef __GNUC__
int sh_module_vsend(int source, int dest, const char *fmt, ...)
__attribute__((format(printf, 3, 4)))
#endif
;

bool sh_module_has(int vhandle, int handle);

int sh_module_minload(int vhandle);
int sh_module_nextload(int vhandle);

struct sh_monitor_handle;
int sh_monitor(const char *name, const struct sh_monitor_handle *h, int *handle);
int sh_handler(const char *name, int flag, int *handle);
int sh_handle_publish(const char *name, int flag);

static inline int
sh_handleid(int nodeid, int moduleid) {
    return ((nodeid & 0xff) << 8) | (moduleid & 0xff);
}

static inline int
sh_moduleid_from_handle(int handle) {
    return handle & 0x00ff;
}

static inline int
sh_nodeid_from_handle(int handle) {
    return (handle >> 8) & 0x00ff;
}

#endif
