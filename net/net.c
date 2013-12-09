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

static const char* STRERROR[] = {
    "close",
    "net error end of file",
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
    int wbuffermax;
    int wbuffersz;
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
        s[i].wbuffermax = INT_MAX;
        s[i].wbuffersz = 0;
    }
    s[max-1].fd = -1;
    return s;
}

static struct socket*
_create_socket(struct net* self, socket_t fd, uint32_t addr, uint16_t port, int wbuffermax, 
        int ud, int ut) {
    if (self->free_socket == NULL)
        return NULL;
    struct socket* s = self->free_socket;
    if (s->fd >= 0)
        self->free_socket = &self->sockets[s->fd];
    else
        self->free_socket = NULL;
    assert(s->mask == 0);
    s->fd = fd;
    s->status = STATUS_SUSPEND;
    s->ud = ud;
    s->ut = ut;
    s->addr = addr;
    s->port = port;
    s->rb = netbuf_alloc_block(self->rpool, s-self->sockets);
    s->wbuffersz = 0;
    s->wbuffermax = wbuffermax;
    if (s->wbuffermax <= 0)
        s->wbuffermax = INT_MAX;
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
    s->wbuffersz = 0;
    s->wbuffermax = INT_MAX;
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
_try_close_socket(struct net* self, struct socket* s) {
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
_get_socket(struct net* self, int id) {
    if (id >= 0 && id < self->max)
        // make sure the socket is opened
        if (self->sockets[id].status != STATUS_INVALID)
            return &self->sockets[id];
    return NULL;
}

bool
net_close_socket(struct net* self, int id, bool force) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL)
        return true;
    if (force) {
        _close_socket(self, s);
        return true;
    } else {
        return _try_close_socket(self, s);
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
net_create(int max, int rbuffer) {
    if (max == 0 || rbuffer == 0)
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
    self->rpool = netbuf_create(max, rbuffer);
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

static int
_read_close(struct socket* s) {
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
_readto(struct net* self, struct socket* s, void* buf, int space, int* e) {
    int error;
    int nbyte;
    assert(space > 0);
    if (s->status == STATUS_HALFCLOSE) {
        *e = _read_close(s);
        if (*e) {
            _close_socket(self, s);
        }
        return -1;
    }
    for (;;) {
        nbyte = _socket_read(s->fd, buf, space);
        if (nbyte < 0) {
            error = _socket_geterror(s->fd);
            if (error == SEAGAIN) {
                *e = 0;
                return 0;
            } else if (error == SEINTR) {
                continue;
            } else {
                _close_socket(self, s);
                *e = NETERR(error);
                return -1;
            }
        } else if (nbyte == 0) {
            // error = _socket_geterror(s->fd); 
            // man not point errno if nbyte == 0
            _close_socket(self, s);
            *e = NET_ERR_EOF;
            return -1;
        } else {
            *e = 0;
            return nbyte;
        } 
    }
}

int
net_readto(struct net* self, int id, void* buf, int space, int* e) {
    struct socket* s = _get_socket(self, id);
    if (s) {
        return _readto(self, s, buf, space, e);
    } 
    return -1; 
}

int
net_read(struct net* self, int id, bool force, struct mread_buffer* buf, int* e) {
    struct socket* s = _get_socket(self, id);
    if (s) {
        struct netbuf_block* rb = s->rb;
        if (!force) {
            int nread = RB_NREAD(rb);
            if (nread > 0) {
                buf->ptr = RB_RPTR(rb);
                buf->sz  = nread;
                return buf->sz;
            }
        }
        void* wptr = RB_WPTR(rb);
        int space  = RB_SPACE(rb);
        //assert(space > 0);
        int nread = _readto(self, s, wptr, space, e);
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
net_dropread(struct net* self, int id, int sz) {
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        return;
    }
    struct netbuf_block* rb = s->rb;
    rb->rptr += sz;
    
    if (rb->wptr == rb->sz) {
        if (rb->rptr < rb->wptr) {
            void* begin = rb+1;
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

int
_send_buffer(struct net* self, struct socket* s) {
    int total = 0;
    while (s->head) {
        struct sbuffer* p = s->head;
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
        return -1;
    }
    struct socket* s = _get_socket(self, id);
    if (s == NULL) {
        return -1;
    }
    if (s->status == STATUS_HALFCLOSE) {
        return -1; // do not send
    }
    int error;
    if (s->head == NULL) {
        int n = _socket_write(s->fd, data, sz);
        if (n >= sz) {
            return 0;
        } else if (n >= 0) {
            data = (char*)data + n;
            sz -= n;
        } else {
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
        struct sbuffer* p = malloc(sizeof(*p) + sz);
        memcpy(p->data, data, sz);
        p->next = NULL;
        p->sz = sz;
        p->ptr = p->data;

        s->head = s->tail = p;
        _subscribe(self, s, s->mask|NET_WABLE);
        return 0;
    } else {
        s->wbuffersz += sz;
        if (s->wbuffersz > s->wbuffermax) {
            error = NET_ERR_WBUFOVER;
            goto errout;
        }
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
errout:
    nm->fd = s->fd;
    nm->connid = s - self->sockets;
    nm->type = NETE_SOCKERR;
    nm->error = NETERR(error);
    nm->ud = s->ud;
    nm->ut = s->ut;
    _close_socket(self, s);
    return 1;
 
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
    struct socket* s = _create_socket(self, fd, addr, port, listens->wbuffermax, 
            listens->ud, listens->ut);
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
net_listen(struct net* self, uint32_t addr, uint16_t port, int wbuffermax, int ud, int ut) {
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

    struct socket* s = _create_socket(self, fd, addr, port, wbuffermax, ud, ut);
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
net_connect(struct net* self, uint32_t addr, uint16_t port, bool block, int wbuffermax, 
        int ud, int ut, struct net_message* nm) {
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

    struct socket* s = _create_socket(self, fd, addr, port, wbuffermax, ud, ut);
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
    int c = 0;
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
                c++;
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
                    oe->fd = s->fd;
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
                    oe->fd = s->fd;
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
                oe->fd = s->fd;
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
