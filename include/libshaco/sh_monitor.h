#ifndef __sh_monitor_h__
#define __sh_monitor_h__

struct sh_node_addr;

struct sh_monitor_handle {
    int start_handle;
    int exit_handle;
};

#define MONITOR_START 0
#define MONITOR_EXIT  1
#define MONITOR_MAX 2

int sh_monitor_register(const char *name, const struct sh_monitor_handle *h);
int sh_monitor_trigger_start(int vhandle, int handle, const struct sh_node_addr *addr);
int sh_monitor_trigger_exit(int vhandle, int handle);

#endif
