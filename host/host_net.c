#include "host_net.h"
#include "host_log.h"
#include "host_service.h"
#include "host_dispatcher.h"
#include "net.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>

#define RDBUFFER_SIZE 64*1024

static struct net* N = NULL;

static void
_dispatch_one(struct net_message* nm) {
    if (nm->type == NETE_INVALID) {
        return;
    }
    int serviceid = nm->ud;
    if (nm->type == NETE_CONN_THEN_READ) {
        nm->type = NETE_CONNECT;
        service_notify_net(serviceid, nm);
        nm->type = NETE_READ;
    }
    if (nm->type == NETE_READ) {
        if (nm->ut == NETUT_TRUST) {
            if (host_dispatcher_publish(nm)) {
                service_notify_net(serviceid, nm);
            }
        } else {
            service_notify_net(serviceid, nm);
        }
    } else {
        service_notify_net(serviceid, nm);
    }
}

static void
_dispatch() {
    struct net_message* all = NULL;
    int n = net_getevents(N, &all);
    int i;
    for (i=0; i<n; ++i) {
        struct net_message* nm = &all[i];
        _dispatch_one(nm);
    }
}

int
host_net_init(int max) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    N = net_create(max, RDBUFFER_SIZE);
    return N != NULL ? 0 : 1;
}

void
host_net_fini() {
    net_free(N);
    N = NULL;
}

int
host_net_listen(const char* addr, uint16_t port, int wbuffermax, int serviceid, int ut) {
    uint32_t ip = inet_addr(addr);
    int err = net_listen(N, ip, port, wbuffermax, serviceid, ut);
    if (err) {
        host_error("listen %s:%u fail: %s", addr, port, host_net_error(err)); 
    } else {
        host_info("listen on %s:%d", addr, port);
    }
    return err;
}

int 
host_net_connect(const char* addr, uint16_t port, bool block, int serviceid, int ut) { 
    uint32_t ip = inet_addr(addr);
    struct net_message nm;
    int n = net_connect(N, ip, port, block, 0, serviceid, ut, &nm);
    if (n > 0) {
        _dispatch_one(&nm);
        return nm.type == NETE_CONNERR;
    }
    return 0;
}

void
host_net_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch();
    }
}

int 
host_net_send(int id, void* data, int sz) {
    struct net_message nm;
    int n = net_send(N, id, data, sz, &nm);
    if (n > 0) {
        _dispatch_one(&nm);
    }
    return n;
}
int 
host_net_readto(int id, void* buf, int space, int* e) {
    return net_readto(N, id, buf, space, e); 
}
void* host_net_read(int id, int sz, int skip, int* e) { 
    return net_read(N, id, sz, skip, e); 
}
void host_net_dropread(int id, int skip) { 
    net_dropread(N, id, skip); 
}
bool host_net_close_socket(int id, bool force) { 
    return net_close_socket(N, id, force); 
}
const char* host_net_error(int err) { 
    return net_error(N, err); 
}
int host_net_max_socket() { 
    return net_max_socket(N); 
}
int host_net_subscribe(int id, bool read) { 
    return net_subscribe(N, id, read); 
}
int host_net_socket_address(int id, uint32_t* addr, uint16_t* port) { 
    return net_socket_address(N, id, addr, port); 
}
int host_net_socket_isclosed(int id) {
    return net_socket_isclosed(N, id);
}
