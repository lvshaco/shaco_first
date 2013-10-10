#ifndef __socket_select_h__
#define __socket_select_h__

#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ud is just for fd map to socket*, and not need to alter net implement,
// the most import is this is just for client develop and no publish.
struct np_state {
    int cap; 
    void** ud; 
    int maxfd;
    fd_set rfds;
    fd_set wfds;
    fd_set rtmp;
    fd_set wtmp;
};

static int
np_init(struct np_state* np, int max) {
    int cap = 1;
    while (cap < max)
        cap *= 2;
    np->cap = cap; 
    np->ud = malloc(sizeof(void*) * cap);
    memset(np->ud, 0, sizeof(void*) * cap);
    np->maxfd = -1;
    FD_ZERO(&np->rfds);
    FD_ZERO(&np->wfds);
    return 0;
}

static void
np_fini(struct np_state* np) {
    FD_ZERO(&np->rfds);
    FD_ZERO(&np->wfds);
    np->maxfd = -1;
    free(np->ud);
    np->ud = NULL;
    np->maxfd = 0;
}

static void
_grow(struct np_state* np, int maxfd) {
    int cap = np->cap;
    while (cap <= maxfd)
        cap *= 2;
    np->ud = realloc(np->ud, sizeof(void*) * cap);
    memset(np->ud + np->cap, 0, sizeof(void*) * (cap - np->cap));
    np->cap = cap;
}

static inline bool 
_isvalid_fd(int fd) {
    // in windows: FD_SETSIZE is the max count of fd
    // in linux:   FD_SETSIZE is the max value of fd, so
    // Executing FD_CLR() or FD_SET() with a value of fd 
    // that is negative or is equal to or larger than 
    // FD_SETSIZE will result in undefined behavior
    if (fd < 0)
        return false;
#ifndef WIN32
    if (fd >= FD_SETSIZE)
        return false;
#endif
    return true;
}

static int
np_add(struct np_state* np, int fd, int mask, void* ud) {
    if (!_isvalid_fd(fd)) {
        return -1;
    }
    bool set = false;
    if (mask & NET_RABLE) {
        FD_SET(fd, &np->rfds);
        set = true;
    }
    if (mask & NET_WABLE) {
        FD_SET(fd, &np->wfds);
        set = true;
    }
    if (set) {
        if (fd >= np->cap) {
            _grow(np, fd);
        }
        np->ud[fd] = ud;
        if (np->maxfd < fd)
            np->maxfd = fd;
    }
    return 0;
}

static int
np_mod(struct np_state* np, int fd, int mask, void* ud) {
    if (!_isvalid_fd(fd)) {
        return -1;
    }
    if (mask & NET_RABLE) {
        FD_SET(fd, &np->rfds);
    } else {
        FD_CLR(fd, &np->rfds);
    }
    if (mask & NET_WABLE) {
        FD_SET(fd, &np->wfds);
    } else {
        FD_CLR(fd, &np->wfds);
    }
    return 0;
}


static int
np_del(struct np_state* np, int fd) {
    if (!_isvalid_fd(fd)) {
        return -1;
    }
    assert(fd < np->cap);
    int i;
    FD_CLR(fd, &np->rfds);
    FD_CLR(fd, &np->wfds);
    np->ud[fd] = NULL;
    if (fd == np->maxfd) {
        for (i = np->maxfd-1; i>=0; --i) {
            if (np->ud[i]) {
                break;
            }
        }
        np->maxfd = i;
    }
    return 0;
}

static int
np_poll(struct np_state* np, struct np_event* e, int max, int timeout) {
    if (np->maxfd == -1)
        return 0;
    memcpy(&np->rtmp, &np->rfds, sizeof(fd_set));
    memcpy(&np->wtmp, &np->wfds, sizeof(fd_set));

    struct timeval tv;
    struct timeval* ptv = NULL;
    if (timeout >= 0) {
        tv.tv_sec = timeout/1000;
        tv.tv_usec = timeout%1000*1000;
        ptv = &tv;
    }
    int maxfd = np->maxfd;
    int i, n = 0;
    int count = select(maxfd+1, &np->rtmp, &np->wtmp, NULL, ptv);
    if (count > 0) {
        for (i=0; i<=maxfd; i++) {
            bool read  = FD_ISSET(i, &np->rtmp);
            bool write = FD_ISSET(i, &np->wtmp);
            if (read || write) {
                if (np->ud[i]) {
                    e[n].ud = np->ud[i];
                    e[n].read = read;
                    e[n].write = write;
                    n++;
                }
            }
        }
    }
    return n; 
}
#endif
