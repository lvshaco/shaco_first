#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <arpa/inet.h>
#include <signal.h>
#else
#include <winsock2.h>
#endif

static struct net* N = NULL;
static int SERVER = -1;

static void
_onconnect(struct net_message* nm) {
    printf("onconnect\n");
    SERVER = nm->connid;
}

static void
_onconnerr(struct net_message* nm) {
    printf("onconnerr: %d, %s\n", net_errorid(N), net_error(N));
    SERVER = -1;
}

static void
_onsockerr(struct net_message* nm) {
    printf("onsockerr: %d, %s\n", net_errorid(N), net_error(N));
    SERVER = -1;
}

static char*
_readone(struct net_message* nm) {
    char* head = net_read(N, SERVER, 4, 0);
    if (head == NULL) {
        goto err;
    }
    int l = head[0] | (head[1] << 8);
    char* data = net_read(N, SERVER, l-4, 0);
    if (data == NULL) {
        goto err;
    }
    return data;
err:
    if (net_socket_isclosed(N, SERVER)) {
        SERVER = -1;
    }
    return NULL;
}

static void
_read(struct net_message* nm) {
    printf("read\n");

    int l;
    char buf[64];
    char* data;
    while ((data = _readone(nm))) {
        l = strlen(data); // unsafe
        memcpy(buf, data, l);
        buf[l] = '\0';
        printf("receive: %s\n", buf);

        net_dropread(N, SERVER, 0);
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

static int 
_connect(const char* ip, uint16_t port) {
    uint32_t addr = inet_addr(ip);
    int n = net_connect(N, addr, port, false, 0, 0);
    if (n <= 0) {
        _dispatch(); 
    }
    return n;
}

static int
_send(int id, void* data, int sz) {
    int r = net_send(N, id, data, sz);
    if (r > 0) {
        _dispatch();
    }
    return r;

}

static int
_poll(int timeout) {
    int n = net_poll(N, timeout);
    if (n > 0) {
        _dispatch();
    }
    return n;
}

int main(int argc, char* argv[]) {
#ifdef WIN32
    WSADATA wd;
    WSAStartup( MAKEWORD(2, 2) , &wd);
#else
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
#endif
    const char* ip = "192.168.1.140";
    uint16_t port = 9999;
    N = net_create(10, 64*1024);
    printf("connec to %s:%u ... \n", ip, port);
    if (_connect(ip, port) < 0) {
        printf("connect fail\n");
        goto exit;
    }

    char buf[64];
    char head[4];
    head[2] = 0;
    head[3] = 0;
    for (;;) {
        _poll(1);
        if (fgets(buf, sizeof(buf), stdin)) {
            if (memcmp(buf, "quit", 4) == 0) {
                printf("logout\n");
                break;
            }
            if (SERVER == -1) {
                continue;
            }
            int l = strlen(buf) + 1;
            l += 4; 
            head[0] = l&0xff;
            head[1] = (l>>8) & 0xff;
            _send(SERVER, head, sizeof(head));
            _send(SERVER, buf, l-4);
        }
    }
    net_free(N);
    printf("hello !\n");
exit:
#ifdef WIN32
    WSACleanup();
#endif
    system("pause");
    return 0;
}
