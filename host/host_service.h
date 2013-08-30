#ifndef __host_service_h__
#define __host_service_h__

#include <stdint.h>

struct service_protocol {
    uint32_t sessionid;
    int source;
    void* msg;
    size_t sz;
};

struct service {
    const char* name;
    void* handle;
    void* (*create)();
    void  (*free)(void* pointer);
    void  (*init)();
    void  (*process)();
};

struct service* service_open(const char* name);
void service_close(struct service* s);

#endif
