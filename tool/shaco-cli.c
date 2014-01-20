#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define TYPE 1001

struct client {
    int fd;
};

static void
_onerror(int fd) {
    printf("!!!socket close: %s.\n", strerror(errno));
    close(fd);
    exit(1);
}

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
            buf += n;
            sz -= n;
        }
    }
    return 0;
err:
    _onerror(fd);
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
            buf += n;
            sz -= n;
        }
    }
    return 0;
err:
    _onerror(fd);
    return 1;
}

#define CHK(fn) if (fn) return 1

static int
_write_one(int fd, const char* buf, int sz, int8_t mode) {
    assert(sz > 0);
    uint8_t head[4];

    int l = sz + 7;
    head[0] = l & 0xff;
    head[1] = (l >> 8) & 0xff;
    head[2] = TYPE && 0xff;
    head[3] = (TYPE >> 8) & 0xff;
    CHK(_write(fd, head, 4));
    CHK(_write(fd, &mode, 1));
    CHK(_write(fd, buf, sz));
    return 0;
}

static int
_read_one(int fd) {
    int l;
    uint8_t head[4];
    CHK(_read(fd, &head, 4));
    l = head[0] | (head[1] << 8);
    char buf[l-5];
    CHK(_read(fd, buf, l-6));
    buf[l-6] = '\0';
    printf("%s\n", buf); 
    return 0;
}

static void*
_input(void* ud) {
    struct client* c = ud;
    int fd = c->fd;
    char buf[1024];
    int l;
    while (fgets(buf, sizeof(buf), stdin)) {
        l = strlen(buf);
        if (l <= 1) {
            continue;
        }
        _write_one(fd, buf, l-1, 0);
    }
    return NULL;
}

static void*
_receive(void* ud) {
    struct client* c = ud;
    int fd = c->fd;
    for (;;) { 
        _read_one(fd);
    }
    return NULL;
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

static int
_start_cmdline_mode(int fd, const char* cmdline) {
    CHK(_write_one(fd, cmdline, strlen(cmdline), 1));
    int times = 0;
    for (;;) {
        CHK(_read_one(fd));
        if (++times > 100) {
            usleep(10);
            times = 0;
        }
    }
    return 0;
}

static int
_start_interactive_mode(int fd) {
    struct client* c;
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

static void
usage(const char* app) {
    printf("usage: %s [-h] [--help] [--addr ip:port] [--cmd cmdline]\n", app);
}

int main(int argc, char* argv[]) {

    const char* addr = "127.0.0.1";
    int port = 18000;
    const char* cmdline = NULL;

    const char* app = argv[0];
    char* c;
    int lastarg;
    int i;
    for (i=1; i<argc; ++i) {
        lastarg = i==argc-1;
        if (!strcmp(argv[i], "--addr") && !lastarg) {
            addr = argv[++i];
            c = strchr(addr, ':');
            if (c) {
                *c = '\0';
                port = strtoul(c+1, NULL, 10);
            }
        } else if (!strcmp(argv[i], "--cmd") && !lastarg) {
            cmdline = argv[++i];
        } else {
            usage(app);
            return 1;
        }
    }
   
    int fd = _connect(addr, port);
    if (fd == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }
    printf("connect to %s:%d\n", addr, port);

    if (cmdline) {
        _start_cmdline_mode(fd, cmdline); 
    } else {
        _start_interactive_mode(fd);
    }

    return 0;
}
