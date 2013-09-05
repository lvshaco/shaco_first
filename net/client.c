#include "net.h"
#include "netbuf.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#pragma pack(1)
struct msg_header {
    uint16_t size;
    uint32_t text[0];
};
#pragma pack()

struct client {
    int conn_id;
    struct netbuf_block* wbuf_b;
    int stat_read;
    int stat_write;
};

struct server {
    struct net* ne;
    struct netbuf* wbuf; 
    struct client* clients;
    struct client* free_client;

    int max;
    int nconnected;
    int nconnectfail;
    int nclosedwrite;
    int nclosedread;

    int rstat;
    int wstat;
};

static struct server* s = NULL;
static int package_size = 1024;

static struct client*
_alloc_clients(int max) {
    int i;
    struct client* c = malloc(max * sizeof(struct client));
    for (i=0; i<max; ++i) {
        c[i].conn_id = i+1;
        c[i].wbuf_b = NULL;
        c[i].stat_read = 0;
        c[i].stat_write = 0;
    }
    c[max-1].conn_id = -1;
    return c;
}

static struct client*
_create_client(struct server* s, int conn_id) {
    struct client* c = s->free_client;
    if (c == NULL)
        return NULL;
        
    int next = c->conn_id;
    if (next == -1) {
        s->free_client = NULL;
    } else
        s->free_client = &s->clients[next];

    c->conn_id = conn_id;
    c->wbuf_b = netbuf_alloc_block(s->wbuf, c - s->clients);
    return c;
}

static void
_free_client(struct server* s, struct client* c) {
    struct client* free = s->free_client;
    if (free == NULL) {
        c->conn_id = -1;     
    } else {
        c->conn_id = free - s->clients;
    }
    netbuf_free_block(s->wbuf, c->wbuf_b);
    c->wbuf_b = NULL;
    c->stat_read = 0;
    c->stat_write = 0;
    s->free_client = c;
}

static inline int 
_is_client_closed(struct client* c) {
    return c->wbuf_b == NULL;
}

void
writecb(int fd, int id, void* data) {
    struct client* c = data;
    if (c == NULL) {
        printf("id %d, NULL client write\n", id);
        return;
    }

    struct netbuf_block* wbuf_b = c->wbuf_b;
    if (wbuf_b->roffset == wbuf_b->woffset) {
        return; // empty
    }
    int error;
    if (wbuf_b->roffset < wbuf_b->woffset) {
        void* buf = (void*)wbuf_b + sizeof(*wbuf_b) + wbuf_b->roffset;
        int nbyte = net_write(s->ne, id, buf, wbuf_b->woffset - wbuf_b->roffset);
        if (nbyte == -1) {
            goto err_out;
        }
        if (nbyte > 0) {
            wbuf_b->roffset += nbyte;
            c->stat_write += nbyte;
            goto out;
        }
    } else {
        void* begin = (void*)wbuf_b + sizeof(*wbuf_b);
        void* buf = begin + wbuf_b->roffset;
        int wsize = wbuf_b->size - wbuf_b->roffset;
        int nbyte = net_write(s->ne, id, buf, wsize);
        if (nbyte == -1) {
            goto err_out;
        }
        if (nbyte == 0) {
            goto out;
        }
        c->stat_write += nbyte;
        if (nbyte < wsize) {
            wbuf_b->roffset += nbyte;
            goto out;
        }
        wbuf_b->roffset = 0;
        wsize = wbuf_b->woffset;
        if (wsize == 0) { 
            goto out;
        }
        nbyte = net_write(s->ne, id, begin, wsize);
        if (nbyte == -1) {
            goto err_out;
        }
        if (nbyte > 0) {
            wbuf_b->roffset += nbyte;
            c->stat_write += nbyte;
            goto out;
        } 
    }
out:
    //printf("write size=%d, woffset=%d roffset=%d\n", 
            //c->stat_write - oldstat, wbuf_b->woffset, wbuf_b->roffset);
    return;
err_out:
    error = net_error(s->ne);
    printf("client %d, write occur error %d\n", id, error);
    _free_client(s, c);
    s->nclosedwrite +=1;
}

void
readcb(int fd, int id, void* data) {
    struct client* c = data;
    if (c == NULL) {
        printf("id %d, NULL client read\n", id);
        return;
    }

    int error = NET_OK;
    for (;;) {
        struct msg_header* h = net_read(s->ne, id, sizeof(struct msg_header));
        if (h == NULL) {
            error = net_error(s->ne);
            if (error == NET_OK) {
                goto out;
            } else {
                goto err_out;
            }
        }
        assert(h->size == package_size);
        
        uint32_t* msg = net_read(s->ne, id, h->size);
        if (msg == NULL) {
            error = net_error(s->ne);
            if (error == NET_OK) {
                goto out;
            } else {
                goto err_out;
            }
        }
        int i;
        for (i=0; i<h->size/sizeof(msg[0]); ++i) {
            assert(msg[i] == id);
        }
        //printf("from client %d, read msg size=%d\n", id, h->size);

        //_handle_msg(c, (void*)h, sizeof(*h) + h->size);

        net_dropread(s->ne, id);

        s->rstat += 1;
    }
out:
    return;
err_out:
    printf("client %d read occur error %d\n", id, error);
    _free_client(s, c);
    s->nclosedread +=1;
    return;
}

void 
_connectcb(int fd, int id, void* data, int error) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    getpeername(fd, (struct sockaddr*)&remote_addr, &len);
    printf("new client %d,%d, %s:%u\n", fd, id, 
        inet_ntoa(remote_addr.sin_addr), 
        ntohs(remote_addr.sin_port));

    if (error == 0) {
        printf("connect ok %d,%d, %s:%u\n", fd, id, 
            inet_ntoa(remote_addr.sin_addr), 
            ntohs(remote_addr.sin_port));
        s->nconnected += 1;
        struct client* c = _create_client(s, id);
        assert(c);
        net_add_event(s->ne, id, NETE_READ|NET_WRITE, readcb, writecb, c);
    } else {
        s->nconnectfail += 1;
        printf("connect failed %u, %s\n", error, strerror(error));
    }
}

int
_start_connect(uint32_t addr, uint16_t port, int max) {
    int i;
    for (i=0; i<max; ++i) {
        int r = net_connect(s->ne, addr, port, 1, _connectcb, NULL);
        if (r != 0) {
            return -1;
        }
    }
    return 0;
}

static inline int
_start_write_client(struct client* c, void* msg, int size) {
    struct netbuf_block* wbuf_b = c->wbuf_b;
    int space;
    if (wbuf_b->roffset > wbuf_b->woffset)
        space = wbuf_b->roffset - wbuf_b->woffset;
    else
        space = wbuf_b->size + wbuf_b->roffset - wbuf_b->woffset;
    space -= 1;
    if (space < size) {
        printf("client %d no wbuf space:%d < size:%d\n", c->conn_id, space, size);
        return -1; // full
    }
    void* begin = (void*)wbuf_b + sizeof(*wbuf_b);
    void* buf = begin + wbuf_b->woffset;

    int size1 = wbuf_b->size - wbuf_b->woffset;
    int size2 = 0;
    if (size1 >= size) {
        size1 = size;
    } else {
        size2 = size - size1;
    }
    memcpy(buf, msg, size1);
    if (size2) {
        memcpy(begin, msg+size1, size2);
        wbuf_b->woffset = size2;
    } else {
        wbuf_b->woffset += size1;
        if (wbuf_b->woffset == wbuf_b->size)
            wbuf_b->woffset = 0;
    }
    c->stat_read+=size;
    s->wstat += 1;
    //printf("to client %d, write msg size=%d, woffset=%d roffset=%d\n", 
            //c->conn_id, size, wbuf_b->woffset, wbuf_b->roffset);
    return 0;
}


void
_start_write(struct server* s) {
    int i;
    int j;
    int size = package_size + sizeof(struct msg_header);
    char data[size];
    struct msg_header* m = (struct msg_header*)data;
    m->size = package_size;

    for (i=0; i<s->max; ++i) {
        struct client* c = &s->clients[i];
        if (_is_client_closed(c)) 
            continue;
        for (j=0; j<m->size/sizeof(m->text[0]); ++j) {
            m->text[j] = c->conn_id;
        }
        if (_start_write_client(c, m, size) != 0) {
            net_close_socket(s->ne, c->conn_id);
            _free_client(s, c); 
            s->nclosedwrite +=1;
        } 
    }
}

static void 
_sigint_handler() {
    printf("sig int\n");
    exit(0);
}

int 
main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage: %s ip:port\n", argv[0]);
        return -1;
    }

    char ip_port[24] = {0};
    strncpy(ip_port, argv[1], sizeof(ip_port));

    uint32_t addr = INADDR_ANY;
    uint16_t port = 0;

    char* tmp = strchr(ip_port, ':');
    if (tmp == NULL) {
        port = strtol(ip_port, NULL, 10);
    } else {
        port = strtol(tmp+1, NULL, 10);
        *tmp = '\0';
        addr = inet_addr(ip_port);
    }

    int max = 10;
    if (argc > 2)
        max = strtol(argv[2], NULL, 10);

    int buf_size = 64;
    if (argc > 3)
        buf_size = strtol(argv[3], NULL, 10);

    if (argc > 4)
        package_size = strtol(argv[4], NULL, 10);

    struct net* ne = net_create(max, 64*1024); 
    struct netbuf* wbuf = netbuf_create(max, buf_size*1024);

    s = malloc(sizeof(struct server));
    s->ne = ne;
    s->wbuf = wbuf; 
    s->clients = _alloc_clients(max);
    s->free_client = &s->clients[0];
    s->max = max;
    s->nconnected = 0;
    s->nconnectfail = 0;
    s->nclosedwrite = 0;
    s->nclosedread = 0;
    s->rstat = 0;
    s->wstat = 0;
    printf("connect to %s\n", argv[1]);
    if (_start_connect(addr, port, max) != 0) {
        printf("connect failed\n");
        return -1;
    }

    signal(SIGINT, _sigint_handler);
    for (;;) {
        int nfd = net_poll(s->ne, 1);
        usleep(100000);
        printf("nfd %d, max %d, connect %d, fail %d, wclose %d, rclose %d,  wstat %d, rstat %d\n", 
                nfd, s->max, s->nconnected, s->nconnectfail, s->nclosedwrite, s->nclosedread, s->wstat, s->rstat);
        _start_write(s);
    }
    net_free(s->ne);
    netbuf_free(s->wbuf);
    free(s->clients);
    free(s);
    return 0;
}
