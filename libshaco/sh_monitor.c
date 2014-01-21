#include "sh_monitor.h"
#include "sc_init.h"
#include "sc_node.h"
#include "sh_util.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct sh_monitor {
    int tar_handle;
    int handle[MONITOR_MAX];
};

static struct {
    int cap;
    int sz;
    struct sh_monitor *p;
} *_M = NULL;

static inline void
_set_handle(struct sh_monitor *m, const struct sh_monitor_handle *h) {
    m->handle[MONITOR_START] = h->start_handle;
    m->handle[MONITOR_EXIT] = h->exit_handle;
}

static struct sh_monitor *
_find(int vhandle) {
    int i;
    for (i=0; i<_M->sz; ++i) {
        if (_M->p[i].tar_handle == vhandle) {
            return &_M->p[i];
        }
    }
    return NULL;
}

int 
sh_monitor_register(const char *name, const struct sh_monitor_handle *h) {
    int vhandle = sc_service_subscribe(name);
    if (vhandle == -1) {
        return -1;
    }
    struct sh_monitor *m = _find(vhandle);
    if (m == NULL) {
        if (_M->sz >= _M->cap) {
            _M->cap *= 2;
            if (_M->cap == 0)
                _M->cap = 1;
            _M->p = realloc(_M->p, sizeof(_M->p[0]) * _M->cap);
        }
        m = &_M->p[_M->sz++];
        m->tar_handle = vhandle;
    }
    _set_handle(m, h);
    return vhandle;
}

int 
sh_monitor_trigger_start(int vhandle, int handle, const struct sh_node_addr *addr) {
    struct sh_monitor *m = _find(vhandle);
    if (m) {
        uint8_t msg[5 + sizeof(struct sh_node_addr)];
        uint8_t *p = msg;
        *p++ = MONITOR_START;
        sh_to_bigendian32(vhandle, p); p+=4;
        memcpy(p, addr->naddr, sizeof(addr->naddr)); p+=sizeof(addr->naddr);
        sh_to_bigendian16(addr->nport, p); p+=2;
        memcpy(p, addr->gaddr, sizeof(addr->gaddr)); p+=sizeof(addr->gaddr);
        sh_to_bigendian16(addr->gport, p); p+=2;
        return sh_service_send(handle, m->handle[MONITOR_START], MT_MONITOR, msg, sizeof(msg));
    }
    return 1;
}

int 
sh_monitor_trigger_exit(int vhandle, int handle) {
    struct sh_monitor *m = _find(vhandle);
    if (m) {
        uint8_t msg[5];
        msg[0] = MONITOR_EXIT;
        sh_to_bigendian32(vhandle, &msg[1]);
        return sh_service_send(handle, m->handle[MONITOR_EXIT], MT_MONITOR, msg, sizeof(msg));
    }
    return 1;
}

static void
sh_monitor_init() {
    _M = malloc(sizeof(*_M));
    memset(_M, 0, sizeof(*_M));
}

static void
sh_monitor_fini() {
    if (_M == NULL)
        return;
    free(_M->p);
    free(_M);
    _M = NULL;
}

SC_LIBRARY_INIT_PRIO(sh_monitor_init, sh_monitor_fini, 40)
