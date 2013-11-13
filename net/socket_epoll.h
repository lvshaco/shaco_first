#ifndef __socket_epoll_h__
#define __socket_epoll_h__

#include <sys/epoll.h>
#include <stdlib.h>

struct np_state {
    int epoll_fd;
    struct epoll_event* ev;
};

static int
np_init(struct np_state* np, int max) {
    int epoll_fd = epoll_create(max+1);
    if (epoll_fd == -1)
        return 1;
    if (_socket_closeonexec(epoll_fd))
        return 1;
    np->epoll_fd = epoll_fd;
    np->ev = malloc(sizeof(struct epoll_event) * max);
    return 0;
}

static void
np_fini(struct np_state* np) {
    if (np->ev) {
        free(np->ev);
        np->ev = NULL;
    }
    if (np->epoll_fd != -1) {
        close(np->epoll_fd);
        np->epoll_fd = -1;
    }
}

static inline int
_op(int epoll_fd, int fd, int op, int mask, void* ud) {
    struct epoll_event e;
    e.events = 0;
    if (mask & NET_RABLE) e.events |= EPOLLIN;
    if (mask & NET_WABLE) e.events |= EPOLLOUT;
    e.data.ptr = ud;
    return epoll_ctl(epoll_fd, op, fd, &e);
}

static int
np_add(struct np_state* np, int fd, int mask, void* ud) {
    return _op(np->epoll_fd, fd, EPOLL_CTL_ADD, mask, ud);
}

static int
np_mod(struct np_state* np, int fd, int mask, void* ud) {
    return _op(np->epoll_fd, fd, EPOLL_CTL_MOD, mask, ud);
}

static int
np_del(struct np_state* np, int fd) {
    struct epoll_event e;
    e.events = 0;
    e.data.ptr = 0;
    return epoll_ctl(np->epoll_fd, EPOLL_CTL_DEL, fd, &e);
}

static int
np_poll(struct np_state* np, struct np_event* e, int max, int timeout) {
    struct epoll_event* ev = np->ev;
    int i;
    int n = epoll_wait(np->epoll_fd, ev, max, timeout);
    for (i=0; i<n; ++i) {
        e[i].ud    = ev[i].data.ptr;
        e[i].read  = (ev[i].events & EPOLLIN) != 0;
        e[i].write = ((ev[i].events & EPOLLOUT) != 0) ||
                     ((ev[i].events & EPOLLERR) != 0) ||
                     ((ev[i].events & EPOLLHUP) != 0);
    }
    return n;
}
#endif
