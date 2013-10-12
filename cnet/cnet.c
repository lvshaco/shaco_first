#include "cnet.h"
#include "net.h"
#include <stdio.h>
#ifndef WIN32
#include <arpa/inet.h>
#include <signal.h>
#else
#include <winsock2.h>
#endif

static struct net* N = NULL;
static cnet_onconn     _onconnect = NULL;
static cnet_onconnerr  _onconnerr = NULL;
static cnet_onsockerr  _onsockerr = NULL;
static cnet_handleum   _handleum  = NULL;
void 
cnet_cb(cnet_onconn onconn, 
        cnet_onconnerr onconnerr, 
        cnet_onsockerr onsockerr, 
        cnet_handleum handleum) {
    if (onconn) _onconnect = onconn;
    if (onconnerr) _onconnerr = onconnerr;
    if (onsockerr) _onsockerr = onsockerr;
    if (handleum) _handleum  = handleum;
}

static void
_onconnectdef(struct net_message* nm) {
    printf("onconnect\n");
}
static void
_onconnerrdef(struct net_message* nm) {
    printf("onconnerr: %d, %s\n", nm->error, net_error(N, nm->error));
}
static void
_onsockerrdef(struct net_message* nm) {
    printf("onsockerr: %d, %s\n", nm->error, net_error(N, nm->error));
}
static void
_handleumdef(int id, int ut, struct UM_BASE* um) {
}

static inline struct UM_BASE*
_readone(struct net_message* nm, int skip) {
    struct UM_BASE* base;
    void* data;
    int id = nm->connid; 
    base = net_read(N, id, sizeof(*base), skip);
    if (base == NULL) {
        goto null;
    }
    int sz = base->msgsz+skip-sizeof(*base);
    if (sz != 0) {
        data = net_read(N, id, sz, 0);
        if (data == NULL) {
            goto null;
        }
    }
    return base;
null:
    if (net_socket_isclosed(N, id)) {
        nm->type = NETE_SOCKERR;
        _onsockerr(nm);
    }
    return NULL;
}

static void
_read(struct net_message* nm) {
    int id = nm->connid;
    struct UM_BASE* um;
    while ((um = _readone(nm, UM_SKIP)) != NULL) {
        um->msgsz += UM_SKIP;
        _handleum(id, nm->ut, um);
        net_dropread(N, id, UM_SKIP);
    }
}

static void
_dispatch() {
    struct net_message* all;
    int i;
    int n = net_getevents(N, &all);
    for (i=0; i<n; ++i) {
        struct net_message* nm = &all[i];
        switch (nm->type) {
        case NETE_READ:
            _read(nm);
            break;
        case NETE_CONNECT:
            _onconnect(nm);
            break;
        case NETE_CONNERR:
            _onconnerr(nm);
            break;
        case NETE_SOCKERR:
            _onsockerr(nm);
            break;
        case NETE_CONN_THEN_READ:
            nm->type = NETE_CONNECT;
            _onconnect(nm);
            nm->type = NETE_READ;
            _read(nm);
            break;
        default:
            break;
        }
    }
}
int 
cnet_connect(const char* ip, uint16_t port, int ut) {
    uint32_t addr = inet_addr(ip);
    return cnet_connecti(addr, port, ut);
}
int  
cnet_connecti(uint32_t addr, uint16_t port, int ut) {
    int n = net_connect(N, addr, port, false, 0, ut);
    if (n <= 0) {
        _dispatch(); 
    }
    return n;
}
int
cnet_send(int id, void* um, int sz) {
    UM_CAST(UM_BASE, m, um);
    m->msgsz = sz-UM_SKIP;
    int r = net_send(N, id, (char*)m+UM_SKIP, m->msgsz);
    if (r > 0) {
        _dispatch();
    }
    return r;

}
int
cnet_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch();
    }
    return n;
}
int  
cnet_init(int cmax) {
#ifdef WIN32
    WSADATA wd;
    WSAStartup( MAKEWORD(2, 2) , &wd);
#else
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
#endif
    cnet_cb(_onconnectdef, 
            _onconnerrdef,
            _onsockerrdef,
            _handleumdef);
    N = net_create(cmax, 64*1024);
    return N != NULL ? 0 : 1;

}
void 
cnet_fini() {
    if (N) {
        net_free(N);
        N = NULL;
#ifdef WIN32
        WSACleanup();
#endif
    }
}
int cnet_subscribe(int id, int read, int write) { 
    return net_subscribe(N, id, read, write); 
}
