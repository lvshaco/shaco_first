#include <sys/epoll.h>

struct np_state {
    int epoll_fd;
    struct epoll_event* events;
};

static int
np_init(struct np_state* np, int max) {
    int epoll_fd = epoll_create(max+1);
    if (epoll_fd == -1)
        return 1;
    if (_set_closeonexec(epoll_fd))
        return 1;
    np->epoll_fd = epoll_fd;
    np->events = malloc(sizeof(struct epoll_event) * max);
    return 0;
}

static void
np_fini(struct np_state* np) {
    if (np->events) {
        free(np->events);
        np->events = NULL;
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
    if (mask & NET_RABLE) e.events |= EPOLL_IN;
    if (mask & NET_WABLE) e.events |= EPOLL_OUT;
    e.data.ptr = ud;
    return epoll_clt(np->epoll_fd, op, fd, &e);
}

static int
np_add(struct np_state* np, int fd, int mask, void* ud) {
    return _op(np->epoll_fd, fd, EPOLL_CTL_ADD, mask, ud);
}

static int
np_mod(struct np_state* np, int fd, int mask, void* u) {
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
np_poll(struct np_state* np, struct event* e, int timtout) {
          
}
