#include "sh_net.h"
#include "sh.h"
#include "sh_init.h"
#include "sh_env.h"
#include "sh_log.h"
#include "sh_module.h"
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
    int moduleid = nm->ud;
    if (nm->type == NETE_CONN_THEN_READ) {
        nm->type = NETE_CONNECT;
        module_net(moduleid, nm);
        nm->type = NETE_READ;
    }
    module_net(moduleid, nm);
    /*
    if (nm->type == NETE_READ) {
        if (nm->ut == NETUT_TRUST) {
            if (sh_dispatcher_publish(nm)) {
                module_notify_net(moduleid, nm);
            }
        } else {
            module_notify_net(moduleid, nm);
        }
    } else {
        module_notify_net(moduleid, nm);
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
sh_net_listen(const char* addr, int port, int wbuffermax, int moduleid, int ut, int *err) {
    return net_listen(N, addr, port, wbuffermax, moduleid, ut, err);
}

int 
sh_net_connect(const char* addr, int port, bool block, int moduleid, int ut) { 
    int err;
    int id = net_connect(N, addr, port, block, 0, moduleid, ut, &err);
    if (id >= 0) {
        struct net_message nm = {
            id, NETE_CONNECT, 0, moduleid, ut 
        };
        _dispatch_one(&nm);
        return 0;
    } else if (id == -1) {
        struct net_message nm = {
            id, NETE_CONNERR, err, moduleid, ut
        };
        _dispatch_one(&nm);
        return 1;
    } else {
        return 0;
    }
}

int 
sh_net_block_connect(const char* addr, int port, int moduleid, int ut, int *err) {
    return net_connect(N, addr, port, true, 0, moduleid, ut, err);
}

void
sh_net_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch();
    }
}

int 
sh_net_send(int id, void* data, int sz) {
    struct net_message nm;
    int n = net_send(N, id, data, sz, &nm);
    if (n > 0) {
        _dispatch_one(&nm);
    }
    return n == 0 ? 0 : 1;
}

int 
sh_net_block_send(int id, void *data, int sz, int *err) {
    return net_block_send(N, id, data, sz, err);
}

int 
sh_net_readto(int id, void* buf, int space, int* e) {
    return net_readto(N, id, buf, space, e); 
}

int
sh_net_read(int id, struct mread_buffer* buf, int* e) { 
    return net_read(N, id, buf, e); 
}

void sh_net_dropread(int id, int sz) { 
    net_dropread(N, id, sz); 
}
bool sh_net_close_socket(int id, bool force) { 
    return net_close_socket(N, id, force); 
}
const char* sh_net_error(int err) { 
    return net_error(N, err); 
}
int sh_net_max_socket() { 
    return net_max_socket(N); 
}
int sh_net_subscribe(int id, bool read) { 
    return net_subscribe(N, id, read); 
}
int sh_net_socket_address(int id, uint32_t* addr, int* port) { 
    return net_socket_address(N, id, addr, port); 
}
int sh_net_socket_isclosed(int id) {
    return net_socket_isclosed(N, id);
}

static void
sh_net_init() {
    int max = sh_getint("sh_connmax", 0);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    N = net_create(max, RDBUFFER_SIZE);
    if (N == NULL) {
        sh_exit("net_create fail, max=%d", max);
    }
}

static void
sh_net_fini() {
    net_free(N);
    N = NULL;
}

SH_LIBRARY_INIT_PRIO(sh_net_init, sh_net_fini, 20);
