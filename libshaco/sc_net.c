#include "sc_net.h"
#include "sc.h"
#include "sc_init.h"
#include "sc_env.h"
#include "sc_log.h"
#include "sc_service.h"
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
        service_net(serviceid, nm);
        nm->type = NETE_READ;
    }
    service_net(serviceid, nm);
    /*
    if (nm->type == NETE_READ) {
        if (nm->ut == NETUT_TRUST) {
            if (sc_dispatcher_publish(nm)) {
                service_notify_net(serviceid, nm);
            }
        } else {
            service_notify_net(serviceid, nm);
        }
    } else {
        service_notify_net(serviceid, nm);
    }*/
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
sc_net_listen(const char* addr, int port, int wbuffermax, int serviceid, int ut, int *err) {
    return net_listen(N, addr, port, wbuffermax, serviceid, ut, err);
}

int 
sc_net_connect(const char* addr, int port, bool block, int serviceid, int ut) { 
    int err;
    int id = net_connect(N, addr, port, block, 0, serviceid, ut, &err);
    if (id >= 0) {
        struct net_message nm = {
            id, NETE_CONNECT, 0, serviceid, ut 
        };
        _dispatch_one(&nm);
        return 0;
    } else if (id == -1) {
        struct net_message nm = {
            id, NETE_CONNERR, err, serviceid, ut
        };
        _dispatch_one(&nm);
        return 1;
    } else {
        return 0;
    }
}

int 
sc_net_block_connect(const char* addr, int port, int serviceid, int ut, int *err) {
    return net_connect(N, addr, port, true, 0, serviceid, ut, err);
}

void
sc_net_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch();
    }
}

int 
sc_net_send(int id, void* data, int sz) {
    struct net_message nm;
    int n = net_send(N, id, data, sz, &nm);
    if (n > 0) {
        _dispatch_one(&nm);
    }
    return n;
}

int 
sc_net_block_send(int id, void *data, int sz, int *err) {
    return net_block_send(N, id, data, sz, err);
}

int 
sc_net_readto(int id, void* buf, int space, int* e) {
    return net_readto(N, id, buf, space, e); 
}

int
sc_net_read(int id, bool force, struct mread_buffer* buf, int* e) { 
    return net_read(N, id, force, buf, e); 
}

void sc_net_dropread(int id, int sz) { 
    net_dropread(N, id, sz); 
}
bool sc_net_close_socket(int id, bool force) { 
    return net_close_socket(N, id, force); 
}
const char* sc_net_error(int err) { 
    return net_error(N, err); 
}
int sc_net_max_socket() { 
    return net_max_socket(N); 
}
int sc_net_subscribe(int id, bool read) { 
    return net_subscribe(N, id, read); 
}
int sc_net_socket_address(int id, uint32_t* addr, int* port) { 
    return net_socket_address(N, id, addr, port); 
}
int sc_net_socket_isclosed(int id) {
    return net_socket_isclosed(N, id);
}

static void
sc_net_init() {
    int max = sc_getint("sc_connmax", 0);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    N = net_create(max, RDBUFFER_SIZE);
    if (N == NULL) {
        sc_exit("net_create fail, max=%d", max);
    }
}

static void
sc_net_fini() {
    net_free(N);
    N = NULL;
}

SC_LIBRARY_INIT_PRIO(sc_net_init, sc_net_fini, 20);
