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
#include <time.h>
#include <signal.h>

#pragma pack(1)
struct msg_header {
    uint16_t size;
};
#pragma pack()

struct client {
    int conn_id;
    struct netbuf_block* wbuf_b;
    int rstat;
    int wstat;
    uint64_t create_time;
};

struct server {
    struct net* ne;
    struct netbuf* wbuf;
    struct client* clients;
    struct client* free_client;

    int max;
    int naccept;
    int nwclosed;
    int nrclosed;
    int nhclosed;

    int rrate;
    int wrate;

    uint32_t this_read;
    uint32_t this_write;

    uint32_t this_read_times;
    uint32_t this_write_times;
};

static uint64_t 
get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec /1000000;
}

static struct server* s = NULL;

static struct client*
_alloc_clients(int max) {
    int i;
    struct client* c = malloc(max * sizeof(struct client));
    for (i=0; i<max; ++i) {
        c[i].conn_id = i+1;
        c[i].wbuf_b = NULL;
        c[i].rstat = 0;
        c[i].wstat = 0;
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
    c->create_time = get_time();
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
    c->rstat = 0;
    c->wstat = 0;
    s->free_client = c;
}

static inline int
_handle_msg(struct client* c, void* msg, int size) {
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
    c->rstat+=size;
    //printf("to client %d, write msg size=%d, woffset=%d roffset=%d\n", 
            //c->conn_id, size, wbuf_b->woffset, wbuf_b->roffset);
    return 0;
}

void
writecb(int fd, int id, void* data) {
    struct client* c = data;
    if (c == NULL) {
        printf("client id=%d fd=%d, NULL client write\n", id, fd);
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
            c->wstat += nbyte;
            
            s->this_write_times++;
            s->this_write += nbyte;
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
        c->wstat += nbyte;
        s->this_write_times++;
        s->this_write += nbyte;

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
            c->wstat += nbyte;

            s->this_write_times++;
            s->this_write += nbyte;

            goto out;
        } 
    }
out:
    //printf("write size=%d, woffset=%d roffset=%d\n", 
            //c->wstat - oldstat, wbuf_b->woffset, wbuf_b->roffset);
    return;
err_out:
    error = net_error(s->ne);
    printf("client id=%d fd=%d, write occur error %d\n", id, fd, error);
    _free_client(s, c);
    s->nwclosed += 1;
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

        s->this_read += sizeof(struct msg_header);
        assert(h->size > 0);
        
        void* msg = net_read(s->ne, id, h->size);
        if (msg == NULL) {
            error = net_error(s->ne);
            if (error == NET_OK) {
                goto out;
            } else {
                goto err_out;
            }
        }
        //printf("from client id=%d fd=%d, read msg size=%d\n", id, fd, h->size);

        s->this_read += h->size;
        if (_handle_msg(c, (void*)h, sizeof(*h) + h->size) != 0) {
            goto err_out2;
        }

        net_dropread(s->ne, id);
        s->this_read_times++;
    }
out:
    return;
err_out:
    printf("client id=%d fd=%d read occur error %d\n", id, fd, error);
    _free_client(s, c);
    s->nrclosed += 1;
    return;
err_out2:
    printf("client id=%d fd=%d handle_msg occur error\n", id, fd);
    net_close_socket(s->ne, id);
    _free_client(s, c);
    s->nhclosed += 1;
    return;

}

void 
listencb(int fd, int id) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    getpeername(fd, (struct sockaddr*)&remote_addr, &len);
    printf("new client %d,%d, %s:%u\n", fd, id, 
        inet_ntoa(remote_addr.sin_addr), 
        ntohs(remote_addr.sin_port));

    s->naccept += 1;
    struct client* c = _create_client(s, id);
    assert(c);
    net_add_event(s->ne, id, NETE_READ|NET_WRITE, readcb, writecb, c);
}

static inline int 
_is_client_closed(struct client* c) {
    return c->wbuf_b == NULL;
}

static void 
_statistics() { 
    uint64_t now = get_time();
    double total_rrate = 0;
    double total_wrate = 0;
    double top_rrate = 0;
    double top_wrate = 0;
    int nclient = 0;
    int i;
    for (i=0; i<s->max; ++i) {
        struct client* c = &s->clients[i];
        if (_is_client_closed(c)) 
            continue;
        uint64_t elapse = now - c->create_time;
        double rrate = 0;
        double wrate = 0;
        if (elapse > 0) {
            rrate = c->rstat / (double)elapse * 1000.0;
            wrate = c->wstat / (double)elapse * 1000.0;
        }
        total_rrate += rrate;
        total_wrate += wrate;
        nclient += 1;
        if (rrate > top_rrate)
            top_rrate = rrate;
        if (wrate > top_wrate)
            top_wrate = wrate;
    }
    if (nclient == 0)
        nclient = 1;
    printf("statistic : rrate %f wrate %f top_rrate %f top_wrate %f\n",
            total_rrate / nclient, total_wrate / nclient,
            top_rrate, top_wrate);
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

    struct net* ne = net_create(max, 64*1024);
    struct netbuf* wbuf = netbuf_create(max, buf_size*1024);

    int r = net_listen(ne, addr, port, listencb);
    if (r != 0) {
        return -1; 
    }
    printf("server start listen on %s, max=%d\n", argv[1], max);

    s = malloc(sizeof(struct server));
    s->ne = ne;
    s->wbuf = wbuf;
    s->clients = _alloc_clients(max);
    s->free_client = &s->clients[0];
    s->max = max;
    s->naccept = 0;
    s->nwclosed = 0;
    s->nrclosed = 0;
    s->nhclosed = 0;

    signal(SIGINT, _sigint_handler);

    uint32_t i = 1;
    uint64_t last_time = get_time();
    for (;;) {
        s->this_read_times = 0;
        s->this_write_times = 0;
        s->this_read = 0;
        s->this_write = 0;

        int nfd = net_poll(s->ne, 1);  
        //_statistics();
        uint64_t now = get_time();
        uint32_t elapse = now - last_time;
        last_time = now;
        uint32_t t = elapse > 10 ? 1 : 10 - elapse; 
        printf("%06u, nfd %d, elapse %u t %u, max %d, accept %d, wclosed %d, rclosed %d, hclosed %d, rtimes %u, wtimes %u, rbytes %u, wbytes %u -- \n", 
                i++, nfd, elapse, t, s->max, s->naccept, s->nwclosed, s->nrclosed, s->nhclosed, s->this_read_times, s->this_write_times, s->this_read, s->this_write); 
        usleep(t*1000);
    }
    net_free(s->ne);
    netbuf_free(s->wbuf);
    free(s->clients);
    free(s);
    return 0;
}
