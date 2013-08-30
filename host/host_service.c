#include "host_service.h"
#include "host_log.h"
#include <stdlib.h>
#include <dlfcn.h>

static void
_dlclose(const name, void* handle) {
    if (dlclose(handle)) {
        host_error("service %s close error: %s.\n", name, dlerror());
    }
}

struct service* 
service_open(const char* name) {
    char tmp[128];
    size_t len = strlen(name);
    if (len >= sizeof(tmp)-10) {
        host_error("service name is too long.\n");
        return NULL;
    }

    strcpy(tmp, name);
    strcpy(tmp+len, ".so");
    void* handle = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        host_error("service %s open error: %s.\n", name, dlerror());
        return NULL;
    }
  
    struct service* s = malloc(sizeof(*s));
    strcpy(tmp+len, "_create");
    s->create = dlsym(handle, tmp);
    strcpy(tmp+len, "_free");
    s->free = dlsym(handle, tmp);
    strcpy(tmp+len, "_init");
    s->init = dlsym(handle, tmp);
    strcpy(tmp+len, "_process");
    s->process = dlsym(handle, tmp);

    if (s->process == NULL) {
        _dlclose(name, handle);
        free(s);
        host_error("service %s not process function.\n", name);
        return NULL;
    }

    s->handle = handle;
    s->name = malloc(len+1);
    strcpy(s->name, name);
    return s;
}

void 
service_close(struct service* s) {
    if (s->handle) {
        _dlclose(s->name, s->handle);
    }
    free(s->name);
    free(s);
}

