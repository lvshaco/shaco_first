#include "host_net.h"
#include "host_log.h"
#include "host_service.h"
#include "host_dispatcher.h"
#include "net.h"
#include <stdlib.h>
#include <arpa/inet.h>

#define RDBUFFER_SIZE 64*1024

static struct net* N = NULL;

static void
_dispatch_events() {
    struct net_message* all = NULL;
    struct net_message* e = NULL;
    int i;
    int n = net_getevents(N, &all);
    for (i=0; i<n; ++i) {
        e = &all[i];
        if (e->type == NETE_INVALID)
            continue;
        int serviceid = e->udata;
        if (e->type == NETE_CONN_THEN_READ) {
            e->type = NETE_CONNECT;
            service_notify_net_message(serviceid, e);
            e->type = NETE_READ;
        }
        if (e->type == NETE_READ) {
            if (host_dispatcher_publish(e)) {
                host_error("no dispatcher");
                service_notify_net_message(serviceid, e);
            }
        } else {
            service_notify_net_message(serviceid, e);
        }
    }
}

int
host_net_init(int max) {
    N = net_create(max, RDBUFFER_SIZE);
    return N != NULL ? 0 : 1;
}

void
host_net_fini() {
    net_free(N);
    N = NULL;
}

int
host_net_listen(const char* addr, uint16_t port, int serviceid) {
    uint32_t ip = inet_addr(addr);
    int r = net_listen(N, ip, port, serviceid);
    if (r) {
        host_error("listen %s:%u fail: %s", addr, port, host_net_error());        
    } else {
        host_info("listen on %s:%d", addr, port);
    }
    return r;
}

int 
host_net_connect(const char* addr, uint16_t port, bool block, int serviceid) {
    host_info("connect to %s:%u ...", addr, port);
    uint32_t ip = inet_addr(addr);
    int r = net_connect(N, ip, port, block, serviceid);
    if (r <= 0) {
        _dispatch_events();
    }
    return r;
}

void
host_net_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch_events();
    }
}

int 
host_net_send(int id, void* data, int sz) { 
    int r = net_send(N, id, data, sz);
    if (r > 0) {
        _dispatch_events();
    }
    return r;
}
void* host_net_read(int id, int sz) { 
    return net_read(N, id, sz); 
}
void host_net_dropread(int id) { 
    net_dropread(N, id); 
}
void host_net_close_socket(int id) { 
    net_close_socket(N, id); 
}
const char* host_net_error() { 
    return net_error(N); 
}
int host_net_max_socket() { 
    return net_max_socket(N); 
}
int host_net_subscribe(int id, bool read, bool write) { 
    return net_subscribe(N, id, read, write); 
}
int host_net_socket_address(int id, uint32_t* addr, uint16_t* port) { 
    return net_socket_address(N, id, addr, port); 
}
