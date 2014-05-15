#ifndef __sh_node_h__
#define __sh_node_h__

#include "sh_module.h"

//#define MT_SYS  0
#define MT_TEXT 1
#define MT_UM   2
#define MT_MONITOR 3
#define MT_LOG 4
#define MT_CMD 5

// publish flag
#define PUB_SER 1
#define PUB_MOD 2
#define PUB_BOTH PUB_SER|PUB_MOD

// subscribe flag
#define SUB_LOCAL  1
#define SUB_REMOTE 2

#define MONITOR_START 0
#define MONITOR_EXIT  1
#define MONITOR_MAX 2
#define MONITOR_STARTB 2
#define MONITOR_STARTE 3
#define MONITOR_SUSPEND 4

#define sh_monitor_type(msg) (((uint8_t*)(msg))[0])
#define sh_monitor_vhandle(msg) sh_from_littleendian32((msg)+1)
#define sh_handleid(nodeid, moduleid) ((((nodeid) & 0xff) << 8) | ((moduleid) & 0xff))
#define sh_moduleid_from_handle(handle) ((handle) & 0x00ff)
#define sh_nodeid_from_handle(handle) (((handle) >> 8) & 0x00ff)

struct sh_node_addr {
    char naddr[40];
    char gaddr[40];
    char waddr[40];
    int  nport;
    int  gport;
};

struct sh_monitor {
    int start_handle;
    int exit_handle;
};

int sh_handle_send(int source, int dest, int type, const void *msg, int sz);
int sh_handle_broadcast(int source, int dest, int type, const void *msg, int sz);

#ifdef __GNUC__
int sh_handle_vsend(int source, int dest, const char *fmt, ...)
__attribute__((format(printf, 3, 4)))
#endif
;

bool sh_handle_exist(int vhandle, int handle);
int sh_handle_minload(int vhandle);
int sh_handle_nextload(int vhandle);

int sh_handle_subscribe(const char *name, int flag, int *handle);
int sh_handle_publish(const char *name, int flag);
int sh_handle_monitor(const char *name, const struct sh_monitor *h, int *vhandle);
int sh_handle_start(const char *name, int handle, const struct sh_node_addr *addr);
int sh_handle_exit(int handle);
int sh_handle_startb(const char *name);
int sh_handle_starte(const char *name);

#endif
