#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <arpa/inet.h>

struct client {
    int fd;
};

static int
_write(int fd, const void* buf, size_t sz) {
    int n;
    while (sz > 0) {
        n = write(fd, buf, sz);
        if (n < 0) {
            if (errno != EINTR) {
                goto err;
            }
        } else {
            printf("write:%d ", n);
            buf += n;
            sz -= n;
        }
    }
    return 0;
err:
    printf("%s", strerror(errno));
    close(fd);
    exit(1);
    return 1;
}

static int
_read(int fd, void* buf, size_t sz) {
    int n;
    while (sz > 0) {
        n = read(fd, buf, sz);
        if (n < 0) {
            if (errno != EINTR) {
                goto err;
            }
        } else if (n == 0) {
            goto err;
        } else {
            printf("read:%d ", n);
            buf += n;
            sz -= n;
        }
    }
    return 0;
err:
    printf("%s", strerror(errno));
    close(fd);
    exit(1);
    return 1;
}

static void*
_input(void* ud) {
    struct client* c = ud;
    int fd = c->fd;
    char buf[32];
    int l;
    char head[4];
    while (fgets(buf, sizeof(buf), stdin)) {
        l = strlen(buf);
        buf[l-1] = '\0';
        l += 4;
        head[0] = l & 0xff;
        head[1] = (l >> 8) & 0xff;
        _write(fd, head, 4);
        _write(fd, buf, l-4);

        printf("write: %s\n", buf); 
    }
    return NULL;
}


static void*
_receive(void* ud) {
    struct client* c = ud;
    int fd = c->fd;
    int l;
    char head[4];
    fflush(stdout);
    for (;;) {
        fflush(stdout);
        _read(fd, &head, 4);
        l = head[0] | ((int)head[1] << 8);
        char buf[l];
        _read(fd, buf, l-4); 
        buf[l-1] = '\0';
        printf("%s\n", buf); 
    }
    return NULL;
}

static inline int
_blocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

static int
_connect(const char* addr, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }
    /*int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flag | O_BLOCK) == -1) {
        close(fd);
        return -1;
    }
    */
    struct sockaddr_in in;
    memset(&in, 0, sizeof(struct sockaddr_in));
    in.sin_family = AF_INET;
    in.sin_port = htons(port);
    in.sin_addr.s_addr = inet_addr(addr);
    if (connect(fd, (struct sockaddr*)&in, sizeof(struct sockaddr))) {
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s ip port\n", argv[0]);
        return 1;
    }

    const char* addr = argv[1];
    int port = strtol(argv[2], NULL, 10);
    struct client* c;
    int fd;
    
    fd = _connect(addr, port);
    if (fd == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }
    c = malloc(sizeof(*c)); 
    c->fd = fd;

    pthread_t pid1;
    pthread_t pid2;
    if (pthread_create(&pid1, NULL, _input, c)) {
        printf("%s\n", strerror(errno));
        free(c);
        return 1;
    }
    if (pthread_create(&pid2, NULL, _receive, c)) {
        printf("%s\n", strerror(errno));
        free(c);
        return 1;
    }
    pthread_join(pid1, NULL);
    pthread_join(pid2, NULL);
    free(c);
    return 0;
}
