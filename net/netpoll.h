#ifndef __netpoll_h__
#define __netpoll_h__

#include <stdbool.h>

#define NET_RABLE 1
#define NET_WABLE 2

struct np_event {
    void* ud;
    bool read;
    bool write;
};

struct np_state;
static int np_init(struct np_state* np, int max);
static void np_fini(struct np_state* np);
static int np_add(struct np_state* np, int fd, int mask, void* ud);
static int np_mod(struct np_state* np, int fd, int mask, void* ud); 
static int np_del(struct np_state* np, int fd); 
static int np_poll(struct np_state* np, struct np_event* e, int max, int timeout);
    
#ifdef __linux__
#include "socket_epoll.h"
#endif

#ifdef WIN32
#include "socket_select.h"
#endif


#endif
