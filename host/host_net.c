#include "host_net.h"
#include "net.h"
#include "host_service.h"
#include <stdlib.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>

#define RDBUFFER_SIZE 64*1024

static struct net* N = NULL;

static void
_dispatch_events() {
    struct net_event* all = NULL;
    struct net_event* e = NULL;
    int i;
    int n = net_getevents(N, &all);
    for (i=0; i<n; ++i) {
        e = &all[i];
        int serviceid = e->udata;
        if (e->type == NETE_CONNECT_THEN_READ) {
            e->type = NETE_CONNECT;
            service_notify_net_message(serviceid, e);
            e->type = NETE_READ;
        }
        service_notify_net_message(serviceid, e);
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
    return net_listen(N, ip, port, serviceid);
}

int 
host_net_connect(const char* addr, uint16_t port, bool block, int serviceid) {
    uint32_t ip = inet_addr(addr);
    int r = net_connect(N, ip, port, block, serviceid);
    if (r > 0) {
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

void* host_net_read(int id, int sz) { return net_read(N, id, sz); }
void host_net_dropread(int id) { net_dropread(N, id); }
int host_net_send(int id, void* data, int sz) { return net_send(N, id, data, sz); }
void host_net_close_socket(int id) { net_close_socket(N, id); }
int host_net_error() { return net_error(N); }

