#ifndef __socket_h__
#define __socket_h__

// include
#ifdef WIN32                 
#define WIN32_LEAN_AND_MEAN  
#include <winsock2.h>        
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

// socket type
#ifndef WIN32
#define fd_t int
#define SOCKET_INVALID -1
#else
#define fd_t SOCKET // intptr_t
#define socklen_t int
#define SOCKET_INVALID INVALID_SOCKET
#endif

// error type 
#ifndef WIN32                                                                     
#define SEINTR EINTR
#define SEAGAIN EAGAIN
#define SECONNECTING(e) ((e) == EINPROGRESS)
#else                                                                             
#define SEINTR WSAEINTR
#define SEAGAIN WSAEWOULDBLOCK
#define SECONNECTING(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS)
#endif

// get error
#ifndef WIN32
#define _socket_error errno
#define _socket_strerror(e) strerror(e)
#define _socket_geterror(fd) errno
#define _socket_write(fd, buf, sz) write(fd, buf, sz)
#define _socket_read(fd, buf, sz)  read(fd, buf, sz)
#else
#define _socket_error WSAGetLastError()
#define _socket_strerror(e) "socket error"
int _socket_geterror(fd_t fd);
#define _socket_write(fd, buf, sz) send(fd, buf, sz, 0)
#define _socket_read(fd, buf, sz)  recv(fd, buf, sz, 0)
#endif

// util function
#ifndef WIN32
static inline int
_socket_close(fd_t fd) {
    return close(fd);
}

static inline int
_socket_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1)
        return -1;
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

static inline int
_socket_closeonexec(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1)
        return -1;
    return fcntl(fd, F_SETFL, flag | FD_CLOEXEC);
}

static inline int
_socket_reuseaddr(int fd) {
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

#else
static inline int
_socket_close(fd_t fd) {
    return closesocket(fd);
}

static inline int
_socket_nonblocking(int fd) {
    u_long nonblocking = 1;                                        
    if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR)
        return -1;
    return 0;
}

static inline int
_socket_closeonexec(int fd) {
    return 0;
}

static inline int
_socket_reuseaddr(int fd) {
    return 0;
}

static inline int
_socket_geterror(fd_t fd) {
    int optval;
    socklen_t optvallen = sizeof(optval); 
    int err = WSAGetLastError();                          
    if (err == WSAEWOULDBLOCK && fd >= 0) {
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&optval, &optvallen))
            return err;
        if (optval)
            return optval;
    }
    return err;
}

#endif

#endif
