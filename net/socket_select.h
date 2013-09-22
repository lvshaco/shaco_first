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
_grow(struct np_state* np) {
    int cap = np->cap;
    np->cap *= 1;
    np->ud = realloc(np->ud, sizeof(void*) * np->cap);
    memset(np->ud + cap, 0, sizeof(void*) * (np->cap - cap));
}

static int
np_add(struct np_state* np, int fd, int mask, void* ud) {
    assert(fd >= 0);
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
            _grow(np);
        }
        np->ud[fd] = ud;
        if (np->maxfd < fd)
            np->maxfd = fd;
    }
    return 0;
}

static int
np_mod(struct np_state* np, int fd, int mask, void* ud) {
    assert(fd >= 0);
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
    assert(fd >= 0);
    assert(fd < np->cap);
    int i;
    FD_CLR(fd, &np->rfds);
    FD_CLR(fd, &np->wfds);
    np->ud[fd] = NULL;
    if (fd == np->maxfd) {
        for (i = np->maxfd-1; i>=0; --i) {
            if (np->ud[fd]) {
                np->maxfd = i;
                break;
            }
        }
    }
    return 0;
}

static int
np_poll(struct np_state* np, struct np_event* e, int max, int timeout) {  
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
    if (select(maxfd+1, &np->rtmp, &np->wtmp, NULL, ptv) > 0) {
        for (i=0; i<=maxfd; i++) {
            bool read  = FD_ISSET(i, &np->rtmp);
            bool write = FD_ISSET(i, &np->wtmp);
            if (read || write) {
                e[n].ud = np->ud[i];
                e[n].read = read;
                e[n].write = write;
                n++;
            }
        }
    }
    return n; 
}
#endif
