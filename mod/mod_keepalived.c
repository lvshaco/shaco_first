#include "sh.h"
#include "args.h"
#include "cmdctl.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#define ST_INVALID      0
#define ST_STARTING     1
#define ST_RUN          2 
#define ST_DISCONNED    3
#define ST_KILLING      4
#define ST_STOP         5
#define ST_MAX          6

static const char *
str_status(int status) {
    static const char *str[] = {
        "invalid",
        "starting",
        "run",
        "disconned",
        "killing",
        "stop",
    };
    assert(status >= ST_INVALID &&
           status <  ST_MAX);
    return str[status];
}

struct client {
    int connid;
    int status;
    pid_t pid;
    char *args;
    uint64_t last_tick;
};

struct keepalived {
    int suspend_time_max;
    int disconn_time_max;
    int cap;
    int sz;
    struct client *p;
};

// client
static struct client *
c_find(struct keepalived *self, int connid) {
    int i;
    for (i=0; i<self->sz; ++i) {
        if (self->p[i].connid == connid) {
            return &self->p[i];
        }
    }
    return NULL;
}

static struct client *
c_create(struct keepalived *self, const char *args) {
    struct client *c;
    int i;
    for (i=0; i<self->sz; ++i) {
        c = &self->p[i];
        if (!strcmp(c->args, args)) {
            return c;
        }
    }
    if (self->sz == self->cap) {
        self->cap *= 2;
        if (self->cap == 0) {
            self->cap = 1;
        }
        self->p = realloc(self->p, sizeof(self->p[0]) * self->cap);
    }
    c = &self->p[self->sz++];
    c->connid = -1;
    c->status = ST_INVALID;
    c->pid = -1;
    c->args = strdup(args);
    c->last_tick = 0; 

    sh_info("Keepalived client(%s) create", c->args);
    return c;
}

static void
c_close_socket(struct keepalived *self, struct client *c) {
    if (c->connid != -1) {
        sh_net_close_socket(c->connid, true);
    }
}

static int
c_kick(struct keepalived *self, struct client *c) {
    sh_info("Keepalived client(%s) kick", c->args);

    c_close_socket(self, c);
    if (c->args) {
        free(c->args);
        c->args = NULL;
    }
    int idx = c - self->p;
    assert(idx >= 0 && idx < self->sz);
    self->sz--;
    if (self->sz > idx) {
        self->p[idx] = self->p[self->sz];
    }
    return idx;
}

static bool
c_check_running(struct keepalived *self, struct client *c) {
    return kill(c->pid, 0) == 0;
}

static void
c_start(struct keepalived *self, struct client *c) {
    if (c->status == ST_STOP) {
        return;
    }
    sh_info("Keepalived client(%s) start", c->args);

    struct args A;
    args_parsestr(&A, 0, c->args);
    if (A.argc >= ARGS_MAX) {
        sh_error("Keepalived client(%s) start fail, too much arg", c->args);
        return;
    }
    c->status = ST_STARTING;
    A.argv[A.argc] = NULL;
    if (sh_fork(A.argv, A.argc+1)) {
        sh_error("Keepalived client(%s) start fail", c->args);
    }
}

static void
c_kill(struct keepalived *self, struct client *c) {
    sh_info("Keepalived client(%d, %s) kill", c->pid, c->args);

    c_close_socket(self, c);
    if (!kill(c->pid, SIGKILL)) {
        c->status = ST_KILLING;
    } else {
        sh_error("Keepalived client(%d, %s) kill err: %s", 
                c->pid, c->args, strerror(errno));
    }
}

static void
c_wait_start(struct keepalived *self, struct client *c, int ms) {
    sh_info("Keepalived client(%d, %s) wait %dms start", c->pid, c->args, ms);

    bool killed = true;
    int n = 0;
    while (c_check_running(self, c)) {
        usleep(1000);
        n ++;
        if (ms != -1 &&
            ms <= n) {
            killed = false;
            break;
        }
    }
    sh_info("Keepalived client(%d, %s) wait %dms", c->pid, c->args, n);
    if (killed) {
        c_start(self, c);
    }
}

static void
c_stop(struct keepalived *self, struct client *c) {
    sh_info("Keepalived client(%d, %s) stop", c->pid, c->args);
    
    c_close_socket(self, c);
    if (!kill(c->pid, SIGINT)) {
        while (c_check_running(self, c)) {
            usleep(1000);
        }
        c->status = ST_STOP;
    } else {
        sh_error("Keepalived client(%d, %s) stop err: %s", 
                c->pid, c->args, strerror(errno));
    }
}

static void
c_started(struct keepalived *self, struct client *c, int connid, pid_t pid) {
    c->status = ST_RUN;
    c->connid = connid;
    c->pid = pid;
    c->last_tick = sh_timer_now();
    sh_info("Keepalived client(%d, %s) started", c->pid, c->args);
}

static void
c_disconned(struct keepalived *self, struct client *c) {
    // may be ST_KILLING, then ST_DISCONNED, so do not back
    if (c->status < ST_DISCONNED)
        c->status = ST_DISCONNED;
    c->connid = -1;
    c->last_tick = sh_timer_now();
    sh_info("Keepalived client(%d, %s) disconned", c->pid, c->args);
}

// keepalived
struct keepalived *
keepalived_create() {
    struct keepalived *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
keepalived_free(struct keepalived *self) {
    int i;
    for (i=0; i<self->sz; ++i) {
        free(self->p[i].args);
    }
    free(self->p);
    free(self);
}

int
keepalived_init(struct module *s) {
    struct keepalived *self = MODULE_SELF;
   
    const char *ip = sh_getstr("keepalive_ip", "0");
    int port       = sh_getint("keepalive_port", 0);
    int err;
    int id = sh_net_listen(ip, port, 0, MODULE_ID, 0, &err);
    if (id == -1) {
        sh_error("Keepalived listen on %s:%d err: %s", ip, port, sh_net_error(err));
        return 1;
    }
    sh_info("Keepalived listen on %s:%d [%d]", ip, port, id);
    
    // 3 times of keepalive_tick
    self->suspend_time_max = sh_getint("keepalive_suspend_time_max", 9);
    if (self->suspend_time_max < 3)
        self->suspend_time_max = 3;
    self->suspend_time_max *= 1000;
    self->disconn_time_max = sh_getint("keepalive_disconn_time_max", 3);
    if (self->disconn_time_max < 1)
        self->disconn_time_max = 1;
    self->disconn_time_max *= 1000;
    sh_timer_register(MODULE_ID, 100);
    return 0;
}

static void
handle(struct module *s, int connid, const void *msg, int sz) {
    struct keepalived *self = MODULE_SELF;

    int ncmd = sz;
    int i;
    for (i=0; i<sz; ++i) {
        if (((char*)msg)[i] == ' ') {
            ncmd = i;
            break;
        }
    }
    struct client *c;
    const char *cmd = msg;
    const char *arg = msg+ncmd+1;
    if (!strncmp(cmd, "HB", ncmd)) {
        c = c_find(self, connid);
        if (c) {
            c->last_tick = sh_timer_now();
        }
        return;
    } 
    if (!strncmp(cmd, "START", ncmd)) {
        struct args A;
        if (args_parsestrl(&A, 2, arg, sz-ncmd) != 2)
            return;

        pid_t pid = strtol(A.argv[0], NULL, 10);
        const char *a = A.argv[1];
        c = c_create(self, a);
        assert(c);
        c_started(self, c, connid, pid);
        return;
    } 
}

static void
c_read(struct module* s, struct net_message* nm) {
    //struct keepalived *self = MODULE_SELF;
    int id = nm->connid;
    int drop, nread, err = 0;
    uint16_t sz;
    struct mread_buffer buf;
    nread = sh_net_read(id, &buf, &err); 
    if (nread > 0) {
        for (;;) {
            if (buf.sz < 2) {
                break;
            }
            sz = sh_from_littleendian16(buf.ptr) + 2;
            if (buf.sz < sz) {
                break;
            }
            handle(s, id, buf.ptr+2, sz-2);
            buf.ptr += sz;
            buf.sz  -= sz;
        }
        drop = nread - buf.sz;
        if (drop) {
            sh_net_dropread(id, drop);
        }
    } else if (nread < 0) {
        goto errout;
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    module_net(nm->ud, nm);
}

void
keepalived_net(struct module* s, struct net_message* nm) {
    struct keepalived *self = MODULE_SELF;
    struct client *c;
    switch (nm->type) {
    case NETE_READ:
        c_read(s, nm);
        break;
    case NETE_ACCEPT:
        sh_net_subscribe(nm->connid, true);
        break;
    case NETE_SOCKERR:
        c = c_find(self, nm->connid);
        if (c) {
            c_disconned(self, c);
            c_wait_start(self, c, 10);
        }
        break;
    }
}

void
keepalived_time(struct module *s) {
    struct keepalived *self = MODULE_SELF;

    uint64_t now = sh_timer_now();

    struct client *c;
    int i;
    for (i=0; i<self->sz; ++i) {
        c = &self->p[i];
        switch (c->status) {
        case ST_INVALID:
            c_start(self, c);
            break;
        case ST_RUN:
            if (now - c->last_tick >= self->suspend_time_max) {
                c_kill(self, c);
                c_wait_start(self, c, 10);
            }
            break;
        case ST_DISCONNED:
            if (c_check_running(self, c)) {
                if (now - c->last_tick >= self->disconn_time_max) {
                    c_kill(self, c);
                    c_wait_start(self, c, 10);
                }
            } else {
                c_start(self, c);
            }
            break;
        case ST_KILLING:
            c_wait_start(self, c, 10);
            break;
        } 
    }
}

static int
command(struct module *s, int source, int connid, const char *msg, int len, struct memrw *rw) {
    struct keepalived *self = MODULE_SELF;
    struct args A;
    args_parsestrl(&A, 0, msg, len);
    if (A.argc == 0) {
        return CTL_ARGLESS;
    }
    struct client *c;
    const char *a;
    int i, n;
    const char *cmd = A.argv[0];
    if (!strcmp(cmd, "list")) {
        for (i=0; i<self->sz; ++i) {
            c = &self->p[i];
            a = c->args ? c->args : "unknown";
            n = snprintf(rw->ptr, RW_SPACE(rw), "\n[%2d] [%s] %d `%s`", 
                    i, str_status(c->status), c->pid, a);
            memrw_pos(rw, n);
        }
    } else if (!strcmp(cmd, "kick")) {
        if (A.argc < 2) {
            return CTL_ARGLESS;
        }
        i = strtol(A.argv[1], NULL, 10);
        if (i >= 0 && i < self->sz) {
            c = &self->p[i];
            c_kick(self, c);
        }
    } else if (!strcmp(cmd, "start")) {
        for (i=0; i<self->sz; ++i) {
            c = &self->p[i];
            if (c->status != ST_RUN) {
                if (c->status == ST_STOP)
                    c->status = ST_INVALID;
                c_start(self, c);
            }
        }
    } else if (!strcmp(cmd, "stop")) {
        for (i=0; i<self->sz; ++i) {
            c = &self->p[i];
            if (c->status == ST_RUN) {
                c_stop(self, c);
            }
        }
    } else {
        return CTL_NOCMD;
    }
    return CTL_OK;
}

void
keepalived_main(struct module* s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_CMD:
        cmdctl(s, source, msg, sz, command);
        break;
    }
}
