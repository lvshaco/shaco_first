#ifndef __socket_h__
#define __socket_h__

#ifndef WIN32
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
#else
static inline int
_set_nonblocking(int fd) {
    u_long nonblocking = 1;                                        
    if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR)
        return -1;
    return 0;
}

static inline int
_set_closeonexec(int fd) {
    return 0;
}

static inline int
_set_reuseaddr(int fd) {
    return 0;
}

#endif

#endif
