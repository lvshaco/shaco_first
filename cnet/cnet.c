#include "cnet.h"
#include "net.h"
#include "message_reader.h"
#include <stdio.h>
#include <string.h>
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

static inline void
mread_throwerr(struct net_message* nm, int e) {
    if (e) {
        nm->type = NETE_SOCKERR;
        nm->error = e;
        _onsockerr(nm);
    }
}

static void
_read(struct net_message* nm) {
    int id = nm->connid;
    int step = 0;
    int drop = 1;
    for (;;) {
        int e = 0;
        struct mread_buffer buf;
        int nread = net_read(N, id, drop==0, &buf, &e);
        if (nread <= 0) {
            mread_throwerr(nm, e);
            return;
        }
        struct UM_CLI_BASE* one;
        while ((one = mread_cli_one(&buf, &e))) {
            UM_DEF(msg, UM_CLI_MAXSZ); 
            msg->nodeid = 0;
            memcpy(&msg->cli_base, one, UM_CLI_SZ(one));

            _handleum(id, nm->ut, msg);
            if (++step > 10) {
                net_dropread(N, id, nread-buf.sz);
                return;
            }
        }
        if (e) {
            net_close_socket(N, id, true);
            mread_throwerr(nm, e);
            return;
        }
        drop = nread - buf.sz;
        net_dropread(N, id, drop);       
    }
}

static inline void
_dispatch_one(struct net_message* nm) {
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

static void
_dispatch() {
    struct net_message* all;
    int i;
    int n = net_getevents(N, &all);
    for (i=0; i<n; ++i) {
        struct net_message* nm = &all[i];
        _dispatch_one(nm);
    }
}

int 
cnet_connect(const char* ip, uint16_t port, int ut) {
    uint32_t addr = inet_addr(ip);
    return cnet_connecti(addr, port, ut);
}
int  
cnet_connecti(uint32_t addr, uint16_t port, int ut) {
    int err;
    int id = net_connect(N, addr, port, false, 0, 0, ut, &err);
    if (id >= 0) {
        struct net_message nm = {
            id, NETE_CONNECT, 0, 0, ut};
        _dispatch_one(&nm); 
        return 0;
    } else if (id == -1) {
        struct net_message nm = {
            -1, NETE_CONNERR, err, 0, ut};
        _dispatch_one(&nm); 
        return 1;
    } else {
        return 0;
    }
}
int
cnet_send(int id, void* um, int sz) {
    struct net_message nm;
    UM_CAST(UM_BASE, m, um);
    m->msgsz = sz;
    int n = net_send(N, id, (char*)m+UM_CLI_OFF, m->msgsz-UM_CLI_OFF, &nm);
    if (n > 0) {
        _dispatch_one(&nm);
    }
    return n;

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
int cnet_subscribe(int id, int read) { 
    return net_subscribe(N, id, read); 
}
int cnet_disconnect(int id) {
    return net_close_socket(N, id, true);
}
