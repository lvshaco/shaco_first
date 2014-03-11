#include "net.h"
#include "netbuf.h"
#include "socket.h"
#include "netpoll.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define STATUS_INVALID    -1
#define STATUS_LISTENING   1 
#define STATUS_CONNECTING  2
#define STATUS_CONNECTED   3
#define STATUS_HALFCLOSE   4
#define STATUS_SUSPEND     5
#define STATUS_OPENED      STATUS_LISTENING

#define LISTEN_BACKLOG 511

#define NETERR(err) (err) != 0 ? (err) : NET_ERR_EOF;

static const char *STRERROR[] = {
    "close",
    "net error end of file",
    "net error msg",
    "net error no socket",
    "net error create socket",
    "net error write buffer over"
    "net error no buffer",
    "net error listen",
    "net error connect",
};

struct sbuffer {
    struct sbuffer *next;
    int sz;
    char *begin;
    char *ptr;
};

struct socket {
    socket_t fd;
    int status;
    int mask;
    int ud;
    int ut;
    struct netbuf_block *rb;
    struct sbuffer *head;
    struct sbuffer *tail; 
    int wbuffermax;
    int wbuffersz;
};

struct net {
    struct np_state np;
    int error;
    int max;
    int nevent;
    struct np_event *ev;
    struct net_message *ne; 
    struct socket *sockets;
    struct socket *free_socket;
    struct socket *tail_socket;
    struct netbuf *rpool;
};

static int
_subscribe(struct net *self, struct socket *s, int mask) {
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
    struct socket *s = malloc(max  *sizeof(struct socket));
    for (i=0; i<max; ++i) {
        s[i].fd = i+1;
        s[i].status = STATUS_INVALID;
        s[i].mask = 0;
        s[i].ud = -1;
        s[i].ut = -1;
        s[i].rb = NULL;
        s[i].head = NULL;
        s[i].tail = NULL;
        s[i].wbuffermax = INT_MAX;
        s[i].wbuffersz = 0;
    }
    s[max-1].fd = -1;
    return s;
}

static struct socket*
_create_socket(struct net *self, socket_t fd, int wbuffermax, int ud, int ut) {
    if (self->free_socket == NULL)
        return NULL;
    struct socket *s = self->free_socket;
    if (s->fd >= 0)
        self->free_socket = &self->sockets[s->fd];
    else
        self->free_socket = NULL;
    s->mask = 0;
    s->fd = fd;
    s->status = STATUS_SUSPEND;
    s->ud = ud;
    s->ut = ut;
    s->rb = netbuf_alloc_block(self->rpool, s-self->sockets);
    s->wbuffersz = 0;
    s->wbuffermax = wbuffermax;
    if (s->wbuffermax <= 0)
        s->wbuffermax = INT_MAX;
    return s;
}

static void
_close_socket(struct net *self, struct socket *s) {
    if (s->status == STATUS_INVALID)
        return;
    _subscribe(self, s, 0);
    _socket_close(s->fd);
    
    s->status = STATUS_INVALID;
    netbuf_free_block(self->rpool, s->rb);
    s->rb = NULL;
   
    struct sbuffer *p = NULL;
    while (s->head) {
        p = s->head;
        s->head = s->head->next;
        free(p->begin);
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

static bool
_try_close_socket(struct net *self, struct socket *s) {
    if (s->status == STATUS_INVALID)
        return true;
    if (s->head) {
        s->status = STATUS_HALFCLOSE; // wait for send
        return false;
    } else {
        _close_socket(self, s);
        return true;
    }
}

static inline struct socket*
_get_socket(struct net *self, int id) {
    if (id >= 0 && id < self->max)
        // make sure the socket is opened
        if (self->sockets[id].status != STATUS_INVALID)
            return &self->sockets[id];
    return NULL;
}

bool
net_close_socket(struct net *self, int id, bool force) {
    struct socket *s = _get_socket(self, id);
    if (s == NULL)
        return true;
    if (force) {
        _close_socket(self, s);
        return true;
    } else {
        return _try_close_socket(self, s);
    }
}

const char *
net_error(struct net *self, int err) {
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
net_max_socket(struct net *self) {
    return self->max;
}

int
net_subscribe(struct net *self, int id, bool read) {
    struct socket *s = _get_socket(self, id);
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
net_create(int max, int rbuffer) {
    if (max == 0 || rbuffer == 0)
        return NULL;

    struct net *self = malloc(sizeof(struct net));
    if (np_init(&self->np, max)) {
        free(self);
        return NULL;
    }
    self->max = max;
    self->nevent = 0;
    self->ev = malloc(max  *sizeof(struct np_event));
    self->ne = malloc(max  *sizeof(struct net_message));
    self->sockets = _alloc_sockets(max);
    self->free_socket = &self->sockets[0];
    self->tail_socket = &self->sockets[max-1];
    self->rpool = netbuf_create(max, rbuffer);
    return self;
}

void
net_free(struct net *self) {
    if (self == NULL)
        return;

    int i;
    for (i=0; i<self->max; ++i) {
        struct socket *s = &self->sockets[i];
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

static int
_read_close(struct socket *s) {
    char buf[1024];
    for (;;) {
        int nbyte = _socket_read(s->fd, buf, sizeof(buf));
        if (nbyte < 0) {
            int error = _socket_geterror(s->fd);
            if (error == SEAGAIN) {
                return 0;
            } else if (error == SEINTR) {
                continue;
            } else {
                return NETERR(error);
            }
        } else if (nbyte == 0) {
            return NET_ERR_EOF;
        } else {
            // if nbyte == sieof(buf) no need read more
            // next time read check, or write check 
            return 0;
        }
    }
    return 0;
}

static int
_readto(struct net *self, struct socket *s, void *buf, int sz, int *err) {
    int error;
    int nbyte;
    assert(sz > 0);
    if (s->status == STATUS_HALFCLOSE) {
        *err = _read_close(s);
        if (*err) {
            _close_socket(self, s);
            return -1;
        } else
            return 0;
    }
    for (;;) {
        nbyte = _socket_read(s->fd, buf, sz);
        if (nbyte < 0) {
            error = _socket_geterror(s->fd);
            if (error == SEAGAIN) {
                *err = 0;
                return 0;
            } else if (error == SEINTR) {
                continue;
            } else {
                _close_socket(self, s);
                *err = NETERR(error);
                return -1;
            }
        } else if (nbyte == 0) {
            // error = _socket_geterror(s->fd); 
            // man not point errno if nbyte == 0
            _close_socket(self, s);
            *err = NET_ERR_EOF;
            return -1;
        } else {
            *err = 0;
            return nbyte;
        } 
    }
}

int
net_readto(struct net *self, int id, void *buf, int sz, int *err) {
    struct socket *s = _get_socket(self, id);
    if (s) {
        return _readto(self, s, buf, sz, err);
    } 
    return -1; 
}

int 
net_block_readto(struct net *self, int id, void *buf, int sz, int *err) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        return 0;
    }
    int n;
    int remain = sz;
    while (remain > 0) {
        n = _socket_read(s->fd, buf, remain);
        if (n < 0) {
            *err = _socket_error;
            if (*err != SEINTR) {
                _close_socket(self, s);
                break;
            }
        } else if (n == 0) {
            *err = NET_ERR_EOF;
            break;
        } else {
            buf += n;
            remain -= n;
        }
    }
    return sz-remain;
}
/*
int
net_read(struct net *self, int id, bool force, struct mread_buffer *buf, int *err) {
    struct socket *s = _get_socket(self, id);
    if (s) {
        struct netbuf_block *rb = s->rb;
        if (!force) {
            int nread = RB_NREAD(rb);
            if (nread > 0) {
                buf->ptr = RB_RPTR(rb);
                buf->sz  = nread;
                return buf->sz;
            }
        }
        void *wptr = RB_WPTR(rb);
        int space  = RB_SPACE(rb);
        //assert(space > 0);
        int nread = _readto(self, s, wptr, space, err);
        if (nread >= 0) {
            rb->wptr += nread;
            buf->ptr = RB_RPTR(rb);
            buf->sz = RB_NREAD(rb);
            return buf->sz;
        }
    }
    return -1;
}

void
net_dropread(struct net *self, int id, int sz) {
    struct socket *s = _get_socket(self, id);
    if (s == NULL) {
        return;
    }
    struct netbuf_block *rb = s->rb;
    rb->rptr += sz;
    
    if (rb->wptr == rb->sz) {
        if (rb->rptr < rb->wptr) {
            void *begin = rb+1;
            int off = rb->wptr - rb->rptr;
            memmove(begin, begin + rb->rptr, off);
            rb->rptr = 0;
            rb->wptr= off;
        } else if (rb->rptr == rb->wptr) {
            rb->rptr = 0;
            rb->wptr = 0;
        } else {
            assert(0);
        }
    }
}
*/

int
net_read(struct net *self, int id, struct mread_buffer *buf, int *err) {
    struct socket *s = _get_socket(self, id);
    if (s == NULL) {
        return -1;
    }
    struct netbuf_block *rb = s->rb;
    assert(rb->rptr == 0);
    void *wptr = RB_WPTR(rb);
    int space  = RB_SPACE(rb);
    if (space == 0) {
        *err = NET_ERR_NOBUF; // full, msg too large
        return -1;
    }
    int nread = _readto(self, s, wptr, space, err);
    if (nread > 0) {
        rb->wptr += nread;
        buf->ptr = RB_RPTR(rb);
        buf->sz = RB_NREAD(rb);
        return buf->sz; // more data, parse it
    }
    return nread; // no more data, check err
}

void
net_dropread(struct net *self, int id, int sz) {
    struct socket *s = _get_socket(self, id);
    if (s == NULL) {
        return;
    }
    assert(sz > 0);
    struct netbuf_block *rb = s->rb;
    assert(rb->rptr == 0);
    rb->rptr += sz; // forward read
    
    int remain = rb->wptr - rb->rptr;
    if (remain > 0) {
        void *begin = rb+1;
        memmove(begin, begin + rb->rptr, remain);
        rb->rptr = 0;
        rb->wptr= remain;
    } else if (remain == 0) {
        rb->rptr = 0;
        rb->wptr = 0;
    } else {
        assert(0);
    }
}

int
_send_buffer(struct net *self, struct socket *s) {
    int total = 0;
    while (s->head) {
        struct sbuffer *p = s->head;
        for (;;) {
            int nbyte = _socket_write(s->fd, p->ptr, p->sz);
            if (nbyte < 0) {
                int error = _socket_geterror(s->fd);
                if (error == SEAGAIN)
                    return 0;
                else if (error == SEINTR) {
                    continue;
                } else {
                    return error;
                }
            } else if (nbyte == 0) {
                return 0;
            } else if (nbyte < p->sz) {
                p->ptr += nbyte;
                p->sz -= nbyte;
                s->wbuffersz -= nbyte;
                return 0;
            } else {
                total += nbyte;
                s->wbuffersz -= nbyte;
                break;
            }
        }
        s->head = p->next;
        free(p->begin);
        free(p);
    }
    if (total > 0 &&
        s->head == NULL) {
        _subscribe(self, s, s->mask & (~NET_WABLE));
    }
    return 0;
}

int 
net_send(struct net* self, int id, void* data, int sz, struct net_message* nm) {
    if (sz <= 0) {
        free(data);
        return -1;
    }
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        free(data);
        return -1;
    }
    if (s->status == STATUS_HALFCLOSE) {
        free(data);
        return -1; // do not send
    }
    int error;
    if (s->head == NULL) {
        char *ptr;
        int n = _socket_write(s->fd, data, sz);
        if (n >= sz) {
            free(data);
            return 0;
        } else if (n >= 0) {
            ptr = (char*)data + n;
            sz -= n;
        } else {
            ptr = data;
            error = _socket_geterror(s->fd);
            switch (error) {
            case SEAGAIN:
                break;
            case SEINTR:
                break;
            default:
                goto errout;
            }
        }
        s->wbuffersz += sz;
        if (s->wbuffersz > s->wbuffermax) {
            error = NET_ERR_WBUFOVER;
            goto errout;
        }
        struct sbuffer* p = malloc(sizeof(*p));
        p->next = NULL;
        p->sz = sz;
        p->begin = data;
        p->ptr = ptr;
        
        s->head = s->tail = p;
        _subscribe(self, s, s->mask|NET_WABLE);
        return 0;
    } else {
        s->wbuffersz += sz;
        if (s->wbuffersz > s->wbuffermax) {
            error = NET_ERR_WBUFOVER;
            goto errout;
        }
        struct sbuffer* p = malloc(sizeof(*p));
        p->next = NULL;
        p->sz = sz;
        p->begin = data;
        p->ptr = data;
        
        assert(s->tail != NULL);
        assert(s->tail->next == NULL);
        s->tail->next = p;
        s->tail = p;
        return 0;
    }
errout:
    free(data);
    nm->connid = s - self->sockets;
    nm->type = NETE_SOCKERR;
    nm->error = NETERR(error);
    nm->ud = s->ud;
    nm->ut = s->ut;
    _close_socket(self, s);
    return 1;

}

int 
net_block_send(struct net* self, int id, void* data, int sz, int *err) {
    struct socket *s = _get_socket(self, id);
    if (s == NULL) {
        *err = NET_ERR_NOSOCK;
        return 0;
    }
    int n;
    int remain = sz;
    while (remain > 0) {
        n = _socket_write(s->fd, data, remain);
        if (n < 0) {
            *err = _socket_error;
            if (*err != SEINTR) {
                _close_socket(self, s);
                break;
            }
        } else {
            data += n;
            remain -= n;
        }
    }
    return sz-remain;
}

static inline struct socket*
_accept(struct net *self, struct socket *listens) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    socket_t fd = accept(listens->fd, (struct sockaddr*)&remote_addr, &len);
    if (fd < 0) {
        return NULL;
    }
    struct socket *s = _create_socket(self, fd, listens->wbuffermax, listens->ud, listens->ut);
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
net_listen(struct net *self, const char *addr, int port, 
        int wbuffermax, int ud, int ut, int *err) {
    char sport[6];
    snprintf(sport, sizeof(sport), "%u", port);
    
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_protocol = IPPROTO_TCP;

    *err = 0;
    int fd = -1;
    int r = getaddrinfo(addr, sport, &hints, &result);
    if (r) {
        *err = NET_ERR_LISTEN;
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        if (_socket_nonblocking(fd) == -1 ||
            _socket_closeonexec(fd) == -1 ||
            _socket_reuseaddr(fd)   == -1) {
            *err = _socket_error;
            _socket_close(fd);
            return -1;
        }

        if (bind(fd, rp->ai_addr, rp->ai_addrlen) == -1) {
            *err = _socket_error;
            _socket_close(fd);
            fd = -1;
            continue;
        }
        break;
    }
    if (fd == -1) {
        if (*err == 0)
            *err = NET_ERR_LISTEN;
        return -1;
    }
    freeaddrinfo(result);
        
    if (listen(fd, LISTEN_BACKLOG) == -1) {
        *err = _socket_error;
        _socket_close(fd);
        return -1;
    }

    struct socket *s = _create_socket(self, fd, wbuffermax, ud, ut);
    if (s == NULL) {
        *err = NET_ERR_CREATESOCK;
        _socket_close(fd);
        return -1;
    }

    if (_subscribe(self, s, NET_RABLE)) {
        *err = _socket_error;
        _close_socket(self, s);
        return -1;
    }
    *err = 0;
    s->status = STATUS_LISTENING;
    return s - self->sockets;
}

static inline int
_onconnect(struct net *self, struct socket *s) {
    int err;
    socklen_t errlen = sizeof(err);
    if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, (void*)&err, &errlen) == -1) {
        if (err == 0)
            err = _socket_error != 0 ? _socket_error : -1;
    }
    if (err == 0) {
        s->status = STATUS_CONNECTED;
        _subscribe(self, s, 0);
    }
    if (err) {
        _close_socket(self, s);
        return err;
    } else {
        return 0;
    }
}

int
net_connect(struct net *self, const char *addr, int port, bool block, 
        int wbuffermax, int ud, int ut, int *err) {
    char sport[6];
    snprintf(sport, sizeof(sport), "%u", port);
    
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_protocol = IPPROTO_TCP;

    *err = 0;
    int status;
    int fd = -1;
    int r = getaddrinfo(addr, sport, &hints, &result);
    if (r) {
        *err = NET_ERR_CONNECT;
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        if (!block)
            if (_socket_nonblocking(fd) == -1) {
                *err = _socket_error;
                return -1;
            }

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == -1) {
            if (block) {
                *err = _socket_error; 
                _socket_close(fd);
                fd = -1;
                continue;
            } else {
                *err = _socket_geterror(fd);
                if (!SECONNECTING(*err)) {
                    _socket_close(fd);
                    fd = -1;
                    continue;
                }
            }
            status = STATUS_CONNECTING;
        } else {
            status = STATUS_CONNECTED;
        }
        if (block)
            if (_socket_nonblocking(fd) == -1) { // 仅connect阻塞
                *err = _socket_error;
                return -1;
            }
        break;
    }
    if (fd == -1) {
        if (*err == 0)
            *err = NET_ERR_CONNECT;
        return -1;
    }
    freeaddrinfo(result);

    struct socket *s = _create_socket(self, fd, wbuffermax, ud, ut);
    if (s == NULL) {
        *err = NET_ERR_CREATESOCK;
        _socket_close(fd);
        return -1;
    }
   
    s->status = status;
    if (s->status == STATUS_CONNECTED) {
        *err = 0;
        return s - self->sockets; // connected
    } else {
        if (_subscribe(self, s, NET_RABLE|NET_WABLE)) {
            *err = _socket_error; 
            _close_socket(self, s);
            return -1;
        } 
        return -2; // connecting
    }
}

int
net_poll(struct net *self, int timeout) {
    int i;
    int n = np_poll(&self->np, self->ev, self->max, timeout);
    int c = 0;
    for (i=0; i<n; ++i) {
        struct np_event *e = &self->ev[i];
        struct socket *s = e->ud;
       
        struct net_message *oe = &self->ne[i];
        oe->type = NETE_INVALID;
        oe->error = 0;
        switch (s->status) {
        case STATUS_LISTENING:
            s = _accept(self, s);
            if (s) {
                oe->connid = s - self->sockets;
                oe->type = NETE_ACCEPT;
                oe->ud = s->ud;
                oe->ut = s->ut;
                c++;
            }
            break;
        case STATUS_CONNECTING:
            if (e->write) {
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                oe->error = _onconnect(self, s);
                if (oe->error) {
                    oe->type = NETE_CONNERR;
                } else {
                    oe->type = NETE_CONNECT;
                    if (e->read) {
                        oe->type = NETE_CONN_THEN_READ;
                    }
                }
                c++;
            }
            break;
        case STATUS_CONNECTED:
        case STATUS_HALFCLOSE:
            if (e->write) {
                oe->error = _send_buffer(self, s);
                if (oe->error) {
                    oe->connid = s - self->sockets;
                    oe->ud = s->ud;
                    oe->ut = s->ut;
                    oe->type = NETE_SOCKERR; 
                    c++;
                    _close_socket(self, s);
                    break;
                }
                if (s->status == STATUS_HALFCLOSE &&
                    s->head == NULL) {
                    oe->connid = s - self->sockets;
                    oe->ud = s->ud;
                    oe->ut = s->ut;
                    oe->type = NETE_WRIDONECLOSE;
                    c++;
                    _close_socket(self, s);
                    break;
                }
            }
            if (e->read) {
                oe->connid = s - self->sockets;
                oe->ud = s->ud;
                oe->ut = s->ut;
                oe->type = NETE_READ;
                c++;
            }
            break;
        }
    }
    self->nevent = c; 
    return c;
}

int
net_getevents(struct net *self, struct net_message **e) {
    *e = self->ne;
    return self->nevent;
}

int 
net_socket_address(struct net *self, int id, uint32_t *addr, int *port) {
    // todo
    struct socket *s = _get_socket(self, id);
    if (s) {
        *addr = 0;
        *port = 0;
        return 0;
    }
    return 1;
}

int 
net_socket_isclosed(struct net *self, int id) {
    struct socket *s = _get_socket(self, id);
    return s == NULL;
}

int 
net_socket_ud(struct net* self, int id, int *ud, int *ut) {
    struct socket *s = _get_socket(self, id);
    if (s) {
        *ud = s->ud;
        *ut = s->ut;
        return 0;
    }
    return 1;
}

int 
net_socket_nonblocking(struct net *self, int id) {
    struct socket *s = _get_socket(self, id);
    if (s)
        return _socket_nonblocking(s->fd);
    return 0;
}
