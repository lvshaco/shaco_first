#include "net.h"
#include "netbuf.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define STATUS_INVALID    -1
#define STATUS_LISTENING   1 
#define STATUS_CONNECTING  2
#define STATUS_CONNECTED   3
#define STATUS_SUSPEND     4 
#define STATUS_OPENED      STATUS_LISTENING

#define LISTEN_BACKLOG 511

#define OK ""
#define NET_ERR_MSG         "net error msg"
#define NET_ERR_NOSOCK      "net error no socket"
#define NET_ERR_CREATESOCK  "net error create socket"

struct sbuffer {
    struct sbuffer* next;
    int sz;
    char* ptr;
    char data[0];
};

struct socket {
    int fd;
    int status;
    int events;
    int ud;
    int ut;
    uint32_t addr;
    uint16_t port;
    struct netbuf_block* rb;
    struct sbuffer* head;
    struct sbuffer* tail; 
};

struct net {
    int epoll_fd;
    
    int max;
    int nevent;
    struct epoll_event* ee;
    struct net_message* ne;  
    struct socket* sockets;
    struct socket* free_socket;

    struct netbuf* rpool; 
    const char* error;
};

static inline int
_add_event(int epoll_fd, struct socket* s, int events) {
    assert(events != 0);
    if (s->events == events) {
        return 0;
    }
    int op = s->events == 0 ? 
        EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    struct epoll_event e;
    e.events = events;
    e.data.ptr = s;
    if (epoll_ctl(epoll_fd, op, s->fd, &e)) {
        return -1;
    } else {
        s->events = events;
        return 0;
    }
}

static inline int
_del_event(int epoll_fd, struct socket* s) {
    if (s->events == 0) {
        return 0;
    }
    struct epoll_event e;
    e.events = 0;
    e.data.ptr = 0;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, s->fd, &e)) {
        return -1;
    } else {
        s->events = 0;
        return 0;
    }
}

static inline int
_set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1)
        return -1;
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

static inline int
_set_closeonexec(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1)
        return -1;
    return fcntl(fd, F_SETFL, flag | FD_CLOEXEC);
}

static inline int
_set_reuseaddr(int fd) {
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

static struct socket*
_alloc_sockets(int max) {
    assert(max > 0);
    int i;
    struct socket* s = malloc(max * sizeof(struct socket));
    for (i=0; i<max; ++i) {
        s[i].fd = i+1;
        s[i].status = STATUS_INVALID;
        s[i].events = 0;
        s[i].ud = -1;
        s[i].ut = -1;
        s[i].addr = 0;
        s[i].port = 0;
        s[i].rb = NULL;
        s[i].head = NULL;
        s[i].tail = NULL;
    }
    s[max-1].fd = -1;
    return s;
}

static struct socket*
_create_socket(struct net* self, int fd, uint32_t addr, uint16_t port, int ud, int ut) {
    if (self->free_socket == NULL)
        return NULL;

    struct socket* s = self->free_socket;
    if (s->fd >= 0)
        self->free_socket = &self->sockets[s->fd];
    else
        self->free_socket = NULL;
    
    s->fd = fd;
    s->status = STATUS_SUSPEND;
    s->ud = ud;
    s->ut = ut;
    s->addr = addr;
    s->port = port;
    s->rb = netbuf_alloc_block(self->rpool, s-self->sockets);
    return s;
}

static void
_close_socket(struct net* self, struct socket* s) {
    if (s->status == STATUS_INVALID)
        return;

    _del_event(self->epoll_fd, s);
    close(s->fd);
    
    s->status = STATUS_INVALID;
    s->events = 0;
    s->ud = -1;
    s->ut = -1;
    s->addr = 0;
    s->port = 0;
    netbuf_free_block(self->rpool, s->rb);
    s->rb = NULL;
   
    struct sbuffer* p = NULL;
    while (s->head) {
        p = s->head;
        s->head = s->head->next;
        free(p);
    }
    s->tail = NULL;

    s->fd = self->free_socket ? self->free_socket - self->sockets : -1;
    self->free_socket = s;
}

static inline struct socket*
_get_socket(struct net* self, int id) {
    if (id >= 0 && id < self->max)
        // make sure the socket is opened
        if (self->sockets[id].status != STATUS_INVALID)
            return &self->sockets[id];
    return NULL;
}

void 
net_close_socket(struct net* self, int id) {
    struct socket* s = _get_socket(self, id);
    if (s) {
        _close_socket(self, s);
    }
}

const char* 
net_error(struct net* self) {
    return self->error;
}

int 
net_max_socket(struct net* self) {
    return self->max;
}

int
net_subscribe(struct net* self, int id, bool read, bool write) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL)
        return -1;

    int events = 0;
    if (read)
        events |= EPOLLIN;
    if (write)
        events |= EPOLLOUT;
    if (events == 0) {
        return _del_event(self->epoll_fd, s);
    } else {
        return _add_event(self->epoll_fd, s, events);
    }
}

struct net*
net_create(int max, int block_size) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    if (max == 0 || block_size == 0)
        return NULL;

    int epoll_fd = epoll_create(max+1);
    if (epoll_fd == -1) {
        return NULL;
    }
    if (_set_closeonexec(epoll_fd) == -1) {
        return NULL;
    }
    struct net* self = malloc(sizeof(struct net));
    self->epoll_fd = epoll_fd;
    self->error = OK;
    self->max = max;
    self->nevent = 0;
    self->ee = malloc(max * sizeof(struct epoll_event));
    self->ne = malloc(max * sizeof(struct net_message));
    self->sockets = _alloc_sockets(max);
    self->free_socket = &self->sockets[0];
    self->rpool = netbuf_create(max, block_size);
    return self;
}

void
net_free(struct net* self) {
    if (self == NULL)
        return;

    int i;
    for (i=0; i<self->max; ++i) {
        struct socket* s = &self->sockets[i];
        if (s->status >= STATUS_OPENED) {
            _close_socket(self, s);
        }
    }
    free(self->sockets);
    free(self->ee);
    free(self->ne);
    netbuf_free(self->rpool);

    close(self->epoll_fd);
    free(self);
}

void*
net_read(struct net* self, int id, int sz, int skip) {
    self->error = OK;
        
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        self->error = NET_ERR_NOSOCK;
        return NULL;
    }

    if (sz <= 0) {
        _close_socket(self, s);
        self->error = NET_ERR_MSG;
        return NULL;
    }

    struct netbuf_block* rb = s->rb;
    void* begin = rb + 1;

    char* rptr = begin + rb->rptr;
    if (rb->wptr - rb->rptr >= sz) {
        rb->rptr += sz;
        return rptr; 
    }

    if (rb->wptr == 0) {
        rb->wptr += skip;
        printf("wptr:%d\n", rb->wptr);
        assert(rb->wptr <= rb->sz);
    }
    char* wptr = begin + rb->wptr;
    int space = rb->sz - rb->wptr;
    if (space < sz) {
        _close_socket(self, s);
        self->error = NET_ERR_MSG;
        return NULL; 
    }

    for (;;) {
        int nbyte = read(s->fd, wptr, space);
        if (nbyte < 0) {
            if (errno == EAGAIN) {
                rb->rptr = 0;
                return NULL;
            } else if (errno == EINTR) {
                continue;
            } else {
                goto err;
            }

        } else if (nbyte == 0) { 
            goto err;
        } else {
            rb->wptr += nbyte;
            if (rb->wptr - rb->rptr >= sz) {
                rb->rptr += sz;
                return rptr;
            } else {
                rb->rptr = 0;
                return NULL;
            }
        } 
        
    }
    return NULL;
err:
    _close_socket(self, s);
    self->error = strerror(errno); // this is valid for nbyte == 0 ?
    return NULL;
}

void
net_dropread(struct net* self, int id, int skip) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL)
        return;
    struct netbuf_block* rb = s->rb;
    assert(rb->rptr >= 0);
    if (rb->rptr == 0)
        return;
    
    int sz = rb->wptr - rb->rptr;
    if (sz == 0) {
        rb->wptr = skip;
        assert(rb->wptr <= rb->sz);
        rb->rptr = 0; 
    } else if (sz > 0) {
        assert(skip < rb->rptr);
        void* begin = rb + 1; 
        memmove(begin+skip, begin + rb->rptr, sz);
        rb->wptr = sz+skip;
        rb->rptr = 0;
    }
    printf("drop wptr:%d\n", rb->wptr);
}

int
_send_buffer(struct net* self, struct socket* s) {
    self->error = OK;
    int total = 0;
    while (s->head) {
        struct sbuffer* p = s->head;
        for (;;) {
            int nbyte = write(s->fd, p->ptr, p->sz);
            if (nbyte < 0) {
                if (errno == EAGAIN)
                    return 0;
                else if (errno == EINTR)
                    continue;
                else {
                    goto err;
                }
            } else if (nbyte == 0) {
                return 0;
            } else if (nbyte < p->sz) {
                p->ptr += nbyte;
                p->sz -= nbyte;
                return nbyte;
            } else {
                total += nbyte;
                break;
            }
        }
        s->head = p->next;
        free(p);
    }
    if (total > 0 &&
        s->head == NULL) {
        net_subscribe(self, s - self->sockets, s->events & EPOLLIN, false);
    }
    return total;
err:
    self->error = strerror(errno);
    _close_socket(self, s);
    return -1;
}

int 
net_send(struct net* self, int id, void* data, int sz) {
    if (sz <= 0) {
        return -1;
    }
    self->error = OK;
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        self->error = NET_ERR_NOSOCK;
        return -1;
    }

    struct net_message* e = &self->ne[0];

    if (s->head == NULL) {
        int n = write(s->fd, data, sz);
        if (n >= sz) {
            return 0;
        } else if (n >= 0) {
            data = (char*)data + n;
            sz -= n;
        } else {
            switch (errno) {
            case EAGAIN:
            case EINTR:
                break;
            default: {
                e->fd = s->fd;
                e->connid = s - self->sockets;
                e->type = NETE_SOCKERR;
                e->ud = s->ud;
                e->ut = s->ut;
                self->nevent = 1;
                self->error = strerror(errno);
                _close_socket(self, s);
                return 1;
            }
            }
        } 
        struct sbuffer* p = malloc(sizeof(*p) + sz);
        memcpy(p->data, data, sz);
        p->next = NULL;
        p->sz = sz;
        p->ptr = p->data;

        s->head = s->tail = p;
        net_subscribe(self, s - self->sockets, s->events & EPOLLIN, true);
        return 0;
    } else {
        struct sbuffer* p = malloc(sizeof(*p) + sz);
        memcpy(p->data, data, sz);
        p->next = NULL;
        p->sz = sz;
        p->ptr = p->data;
        assert(s->tail != NULL);
        assert(s->tail->next == NULL);
        s->tail->next = p;
        s->tail = p;
        return 0;
    }
}

static inline struct socket*
_accept(struct net* self, struct socket* listens) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    int fd = accept(listens->fd, (struct sockaddr*)&remote_addr, &len);
    if (fd == -1) {
        return NULL;
    }
    uint32_t addr = remote_addr.sin_addr.s_addr;
    uint16_t port = ntohs(remote_addr.sin_port);
    struct socket* s = _create_socket(self, fd, addr, port, listens->ud, listens->ut);
    if (s == NULL) {
        close(fd);
        return NULL;
    }

    if (_set_nonblocking(fd) == -1) {
        _close_socket(self, s);
        return NULL;
    }
    s->status = STATUS_CONNECTED;
    return s;
}

int
net_listen(struct net* self, uint32_t addr, uint16_t port, int ud, int ut) {
    self->error = OK;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        self->error = strerror(errno);
        return -1;
    }

    if (_set_nonblocking(fd) == -1 ||
        _set_closeonexec(fd) == -1 ||
        _set_reuseaddr(fd)   == -1) {
        self->error = strerror(errno);
        close(fd);
        return -1;
    }
    
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = addr;
    if (bind(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1) {
        self->error = strerror(errno);
        close(fd);
        return -1;
    }   

    if (listen(fd, LISTEN_BACKLOG) == -1) {
        self->error = strerror(errno);
        close(fd);
        return -1;
    }

    struct socket* s = _create_socket(self, fd, addr, port, ud, ut);
    if (s == NULL) {
        self->error = NET_ERR_CREATESOCK;
        close(fd);
        return -1;
    }

    if (_add_event(self->epoll_fd, s, EPOLLIN)) {
        self->error = strerror(errno);
        _close_socket(self, s);
        return -1;
    }
    s->status = STATUS_LISTENING;
    return 0;
}

static inline int
_onconnect(struct net* self, struct socket* s) {
    int err;
    socklen_t errlen = sizeof(err);
    if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
        if (err == 0)
            err = errno != 0 ? errno : -1;
    }
    if (err == 0) {
        s->status = STATUS_CONNECTED;
    }
    if (err) {
        _close_socket(self, s);
        return err;
    } else {
        return 0;
    }
}

int
net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int ud, int ut) {
    self->error = OK;
    struct net_message* e = &self->ne[0];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        self->error = strerror(errno);
        goto err;
    }
 
    if (!block)
        if (_set_nonblocking(fd) == -1) {
            self->error = strerror(errno);
            goto err;
        }

    int status;
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = addr;
    int r = connect(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    if (r == -1) {
        if (block || errno != EINPROGRESS) {
            self->error = strerror(errno); 
            close(fd);
            goto err;
        }
        status = STATUS_CONNECTING;
    } else {
        status = STATUS_CONNECTED;
    }

    if (block)
        if (_set_nonblocking(fd) == -1) { // 仅connect阻塞
            self->error = strerror(errno);
            goto err;
        }

    struct socket* s = _create_socket(self, fd, addr, port, ud, ut);
    if (s == NULL) {
        self->error = NET_ERR_CREATESOCK;
        close(fd);
        goto err;
    }
   
    s->status = status;
    if (s->status == STATUS_CONNECTED) { 
        goto ok;
    } else {
        if (_add_event(self->epoll_fd, s, EPOLLIN|EPOLLOUT)) {
            self->error = strerror(errno);
            _close_socket(self, s);
            goto err;
        } 
        return 1; // connecting
    }
err:
    e->fd = -1;
    e->connid = -1;
    e->type = NETE_CONNERR;
    e->ud = ud;
    e->ut = ut;
    self->nevent = 1;
    return -1;
ok:
    e->fd = fd;
    e->connid = s - self->sockets;
    e->type = NETE_CONNECT;
    e->ud = ud;
    e->ut = ut;
    self->nevent = 1;
    return 0; // connected
}

int
net_poll(struct net* self, int timeout) {
    int i;
    int nfd = epoll_wait(self->epoll_fd, self->ee, self->max, timeout); 
    for (i=0; i<nfd; ++i) {
        struct epoll_event* e = &self->ee[i];
        struct socket* s = e->data.ptr;
       
        struct net_message* oe = &self->ne[i];
        oe->type = NETE_INVALID;

        switch (s->status) {
        case STATUS_LISTENING:
            s = _accept(self, s);
            if (s) {
                oe->fd = s->fd;
                oe->connid = s - self->sockets;
                oe->type = NETE_ACCEPT;
                oe->ud = s->ud;
                oe->ut = s->ut;
            }
            break;
        case STATUS_CONNECTING:
            if (e->events & EPOLLOUT) {
                oe->fd = s->fd;
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                if (_onconnect(self, s)) {
                    oe->type = NETE_CONNERR;
                    break;
                }
                oe->type = NETE_CONNECT;
                if (e->events & EPOLLIN) {
                    oe->type = NETE_CONN_THEN_READ;
                }
            }
            break;
        case STATUS_CONNECTED:
            if (e->events & EPOLLOUT) {
                if (_send_buffer(self, s) < 0) {
                    oe->fd = s->fd;
                    oe->connid = s - self->sockets;
                    oe->ud = s->ud;
                    oe->ut = s->ut;
                    oe->type = NETE_SOCKERR;
                    break;
                }
            }
            if (e->events & EPOLLIN) {
                oe->fd = s->fd;
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                oe->type = NETE_READ;
            }
            break;
        }
    }
    self->nevent = nfd;
    return nfd;
}

int
net_getevents(struct net* self, struct net_message** e) {
    *e = self->ne;
    return self->nevent;
}

int 
net_socket_address(struct net* self, int id, uint32_t* addr, uint16_t* port) {
    struct socket* s = _get_socket(self, id);
    if (s) {
        *addr = s->addr;
        *port = s->port;
        return 0;
    }
    return 1;
}
