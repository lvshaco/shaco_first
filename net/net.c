#include "net.h"
#include "netbuf.h"
#include "socket.h"
#include "netpoll.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STATUS_INVALID    -1
#define STATUS_LISTENING   1 
#define STATUS_CONNECTING  2
#define STATUS_CONNECTED   3
#define STATUS_SUSPEND     4 
#define STATUS_OPENED      STATUS_LISTENING

#define LISTEN_BACKLOG 511

// must be negative, positive for system error number
//#define OK 0
#define NET_ERR_UNKNOW      -1
#define NET_ERR_MSG         -2
#define NET_ERR_NOSOCK      -3
#define NET_ERR_CREATESOCK  -4
#define NET_ERR_NOBUF       -5
#define NETERR(err) (err) != 0 ? (err) : NET_ERR_UNKNOW;

static const char* STRERROR[] = {
    "close",
    "net error unknow",
    "net error msg",
    "net error no socket",
    "net error create socket",
    "net error no buffer",
};

struct sbuffer {
    struct sbuffer* next;
    int sz;
    char* ptr;
    char data[0];
};

struct socket {
    socket_t fd;
    int status;
    int mask;
    int ud;
    int ut;
    uint32_t addr;
    uint16_t port;
    struct netbuf_block* rb;
    struct sbuffer* head;
    struct sbuffer* tail; 
};

struct net {
    struct np_state np;
    int error;
    int max;
    int nevent;
    struct np_event* ev;
    struct net_message* ne; 
    struct socket* sockets;
    struct socket* free_socket;
    struct socket* tail_socket;
    struct netbuf* rpool; 
};

static int
_subscribe(struct net* self, struct socket* s, int mask) {
    int result;
    if (mask == 0) {
        if (s->mask == 0) {
            return 0;
        }
        result = np_del(&self->np, s->fd);
    } else {
        if (s->mask == mask) {
            return 0;
        }
        if (s->mask == 0) {
            result = np_add(&self->np, s->fd, mask, s);
        } else {
            result = np_mod(&self->np, s->fd, mask, s);
        }
    }
    if (result == 0) {
        s->mask = mask;
    }
    return result;
}

static struct socket*
_alloc_sockets(int max) {
    assert(max > 0);
    int i;
    struct socket* s = malloc(max * sizeof(struct socket));
    for (i=0; i<max; ++i) {
        s[i].fd = i+1;
        s[i].status = STATUS_INVALID;
        s[i].mask = 0;
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
_create_socket(struct net* self, socket_t fd, uint32_t addr, uint16_t port, int ud, int ut) {
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
    _subscribe(self, s, 0);
    _socket_close(s->fd);
    
    s->status = STATUS_INVALID;
    s->mask = 0;
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

    if (self->free_socket == NULL) {
        self->free_socket = s;
    } else {
        assert(self->tail_socket);
        assert(self->tail_socket->fd == -1);
        self->tail_socket->fd = s - self->sockets;
    }
    s->fd = -1;
    self->tail_socket = s;
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
net_error(struct net* self, int err) {
    if (err <= 0) {
        int i = -err;
        if (i>=0 && i<sizeof(STRERROR)/sizeof(STRERROR[0]))
            return STRERROR[i];
        else
            return "";
    } else {
        return _socket_strerror(err);
    }
}

int 
net_max_socket(struct net* self) {
    return self->max;
}

int
net_subscribe(struct net* self, int id, bool read) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL)
        return -1;

    int mask = 0;
    if (read)
        mask |= NET_RABLE;
    if (s->mask & NET_WABLE)
        mask |= NET_WABLE;
    return _subscribe(self, s, mask);
}

struct net*
net_create(int max, int block_size) {
    if (max == 0 || block_size == 0)
        return NULL;

    struct net* self = malloc(sizeof(struct net));
    if (np_init(&self->np, max)) {
        free(self);
        return NULL;
    }
    self->max = max;
    self->nevent = 0;
    self->ev = malloc(max * sizeof(struct np_event));
    self->ne = malloc(max * sizeof(struct net_message));
    self->sockets = _alloc_sockets(max);
    self->free_socket = &self->sockets[0];
    self->tail_socket = &self->sockets[max-1];
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
    self->free_socket = NULL;
    self->tail_socket = NULL;
    free(self->ev);
    free(self->ne);
    netbuf_free(self->rpool);

    np_fini(&self->np);
    free(self);
}

void*
net_read(struct net* self, int id, int sz, int skip, int* e) {
    *e = 0;
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        return NULL;
    }

    if (sz <= 0) {
        _close_socket(self, s);
        *e = NET_ERR_MSG;
        return NULL;
    }

    struct netbuf_block* rb = s->rb;
    void* begin = rb + 1;

    char* rptr = begin + rb->rptr;
    if (rb->wptr - rb->rptr >= sz) {
        rb->rptr += sz;
        return rptr; // has read
    }

    if (rb->sz - rb->rptr < sz) {
        _close_socket(self, s);
        *e = NET_ERR_MSG;
        return NULL;

    }
    if (rb->wptr == 0) {
        rb->wptr += skip;
        assert(rb->wptr <= rb->sz);
    }
    char* wptr = begin + rb->wptr;
    int space = rb->sz - rb->wptr;
    if (space <= 0) {
        _close_socket(self, s);
        *e = NET_ERR_NOBUF;
        return NULL; 
    }

    int error = 0;
    for (;;) {
        int nbyte = _socket_read(s->fd, wptr, space);
        if (nbyte < 0) {
            error = _socket_geterror(s->fd);
            if (error == SEAGAIN) {
                rb->rptr = 0;
                return NULL;
            } else if (error == SEINTR) {
                error = 0;
                continue;
            } else {
                _close_socket(self, s);
                *e = NETERR(error);
                return NULL;
            }
        } else if (nbyte == 0) {
            error = _socket_geterror(s->fd); // errno == 0
            _close_socket(self, s);
            *e = NETERR(error);
            return NULL;
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
}

int
_send_buffer(struct net* self, struct socket* s) {
    int error = 0;
    int total = 0;
    while (s->head) {
        struct sbuffer* p = s->head;
        for (;;) {
            int nbyte = _socket_write(s->fd, p->ptr, p->sz);
            if (nbyte < 0) {
                error = _socket_geterror(s->fd);
                if (error == SEAGAIN)
                    return 0;
                else if (error == SEINTR) {
                    error = 0;
                    continue;
                } else {
                    goto err;
                }
            } else if (nbyte == 0) {
                return 0;
            } else if (nbyte < p->sz) {
                p->ptr += nbyte;
                p->sz -= nbyte;
                return 0;
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
        _subscribe(self, s, s->mask & (~NET_WABLE));
    }
    return 0;
err:
    _close_socket(self, s);
    return error;
}

int 
net_send(struct net* self, int id, void* data, int sz, struct net_message* nm) {
    if (sz <= 0) {
        return -1;
    }
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        return -1;
    }

    if (s->head == NULL) {
        int n = _socket_write(s->fd, data, sz);
        if (n >= sz) {
            return 0;
        } else if (n >= 0) {
            data = (char*)data + n;
            sz -= n;
        } else {
            int error = _socket_geterror(s->fd);
            switch (error) {
            case SEAGAIN:
                break;
            case SEINTR:
                break;
            default: {
                nm->fd = s->fd;
                nm->connid = s - self->sockets;
                nm->type = NETE_SOCKERR;
                nm->error = NETERR(error);
                nm->ud = s->ud;
                nm->ut = s->ut;
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
        _subscribe(self, s, s->mask|NET_WABLE);
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
    socket_t fd = accept(listens->fd, (struct sockaddr*)&remote_addr, &len);
    if (fd < 0) {
        return NULL;
    }
    uint32_t addr = remote_addr.sin_addr.s_addr;
    uint16_t port = ntohs(remote_addr.sin_port);
    struct socket* s = _create_socket(self, fd, addr, port, listens->ud, listens->ut);
    if (s == NULL) {
        _socket_close(fd);
        return NULL;
    }

    if (_socket_nonblocking(fd) == -1) {
        _close_socket(self, s);
        return NULL;
    }
    s->status = STATUS_CONNECTED;
    return s;
}

int
net_listen(struct net* self, uint32_t addr, uint16_t port, int ud, int ut) {
    int error = 0;
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = _socket_error;
        return NETERR(error);
    }

    if (_socket_nonblocking(fd) == -1 ||
        _socket_closeonexec(fd) == -1 ||
        _socket_reuseaddr(fd)   == -1) {
        error = _socket_error;
        _socket_close(fd);
        return NETERR(error);
    }
    
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = addr;
    if (bind(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1) {
        error = _socket_error;
        _socket_close(fd);
        return NETERR(error);
    }   

    if (listen(fd, LISTEN_BACKLOG) == -1) {
        error = _socket_error;
        _socket_close(fd);
        return NETERR(error);
    }

    struct socket* s = _create_socket(self, fd, addr, port, ud, ut);
    if (s == NULL) {
        error = NET_ERR_CREATESOCK;
        _socket_close(fd);
        return NETERR(error);
    }

    if (_subscribe(self, s, NET_RABLE)) {
        error = _socket_error;
        _close_socket(self, s);
        return NETERR(error);
    }
    s->status = STATUS_LISTENING;
    return 0;
}

static inline int
_onconnect(struct net* self, struct socket* s) {
    int err;
    socklen_t errlen = sizeof(err);
    if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, (void*)&err, &errlen) == -1) {
        if (err == 0)
            err = _socket_error != 0 ? _socket_error : -1;
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
net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int ud, int ut, struct net_message* nm) {
    int error;
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = _socket_error;
        goto err;
    }
    if (!block)
        if (_socket_nonblocking(fd) == -1) {
            error = _socket_error;
            goto err;
        }

    int status;
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = addr;
    int r = connect(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    if (r < 0) {
        if (block) {
            error = _socket_error; 
            _socket_close(fd);
            goto err;
        } else {
            error = _socket_geterror(fd);
            if (!SECONNECTING(error)) {
                _socket_close(fd);
                goto err;
            }
        }
        status = STATUS_CONNECTING;
    } else {
        status = STATUS_CONNECTED;
    }

    if (block)
        if (_socket_nonblocking(fd) == -1) { // 仅connect阻塞
            error = _socket_error;
            goto err;
        }

    struct socket* s = _create_socket(self, fd, addr, port, ud, ut);
    if (s == NULL) {
        error = NET_ERR_CREATESOCK;
        _socket_close(fd);
        goto err;
    }
   
    s->status = status;
    if (s->status == STATUS_CONNECTED) { 
        goto ok;
    } else {
        if (_subscribe(self, s, NET_RABLE|NET_WABLE)) {
            error = _socket_error; 
            _close_socket(self, s);
            goto err;
        } 
        return 0; // connecting
    }
err:
    nm->fd = -1;
    nm->connid = -1;
    nm->type = NETE_CONNERR;
    nm->error = error;
    nm->ud = ud;
    nm->ut = ut;
    return 1; // connerr
ok:
    nm->fd = fd;
    nm->connid = s - self->sockets;
    nm->type = NETE_CONNECT;
    nm->error = 0;
    nm->ud = ud;
    nm->ut = ut;
    return 1; // connected
}

int
net_poll(struct net* self, int timeout) {
    int i;
    int n = np_poll(&self->np, self->ev, self->max, timeout);
    for (i=0; i<n; ++i) {
        struct np_event* e = &self->ev[i];
        struct socket* s = e->ud;
       
        struct net_message* oe = &self->ne[i];
        oe->type = NETE_INVALID;
        oe->error = 0;
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
            if (e->write) {
                oe->fd = s->fd;
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                oe->error = _onconnect(self, s);
                if (oe->error) {
                    oe->type = NETE_CONNERR;
                    break;
                }
                oe->type = NETE_CONNECT;
                if (e->read) {
                    oe->type = NETE_CONN_THEN_READ;
                }
            }
            break;
        case STATUS_CONNECTED:
            if (e->write) {
                oe->error = _send_buffer(self, s);
                if (oe->error) {
                    oe->fd = s->fd;
                    oe->connid = s - self->sockets;
                    oe->ud = s->ud;
                    oe->ut = s->ut;
                    oe->type = NETE_SOCKERR;
                    break;
                }
            }
            if (e->read) {
                oe->fd = s->fd;
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                oe->type = NETE_READ;
            }
            break;
        }
    }
    self->nevent = n;
    return n;
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

int 
net_socket_isclosed(struct net* self, int id) {
    struct socket* s = _get_socket(self, id);
    return s == NULL;
}
