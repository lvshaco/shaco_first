#include "sh.h"
#include "hashid.h"
#include "freeid.h"
#include "msg_server.h"
#include "msg_client.h"

struct client {
    int connid;
    bool connected;
};

struct benchmark {
    struct freeid fi;
    struct client* clients;
    int max;
    int connected;
    int query;
    int query_first;
    int query_send;
    int query_recv;
    int query_done;
    int packetsz;
    int packetsplit;
    uint64_t start;
    uint64_t end; 
};

struct benchmark*
benchmark_create() {
    struct benchmark* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
benchmark_free(struct benchmark* self) {
    if (self == NULL)
        return;

    freeid_fini(&self->fi);
    free(self->clients);
    free(self);
}

static int
connect_clients(struct module* s) {
    struct benchmark* self = MODULE_SELF;
    const char* ip = sh_getstr("echo_ip", "");
    int port = sh_getint("echo_port", 0);
    int count = 0;
    int i;
    for (i=0; i<self->max; ++i) { 
        if (sh_net_connect(ip, port, true, MODULE_ID, 0) == 0) {
            count++;
        }
    }
    sh_info("connected client count: %d", count);
    return count;
}

static void
send_one(struct benchmark* self, int id) {
    int sz = self->packetsz;
    int split = self->packetsplit;

    sh_net_subscribe(id, true);
    uint8_t *text = malloc(sz+2);
    sh_to_littleendian16(sz, text);
    sh_to_littleendian16(IDUM_TEXT, text+2);
    memset(text+4, 0, sz-2);
    if (split == 0) {
        sh_net_send(id, text, sz+2);
    } else {/* todo
        sz -= UM_CLI_OFF;
        int i;
        int step = sz/split;
        if (step <= 0) step = 1;
        char* ptr = (char*)text;
        for (i=0; i<sz; i+=step) {
            if (i+step > sz) {
                step = sz - i;
            }
            sh_net_send(id, ptr+i, step); 
            usleep(10000);
            //sh_error("send i %d", i);
        }*/
    }
    self->query_send++;
}

static void
start_send(struct benchmark* self) {
    self->start = sh_timer_now();
    int i, n;
    for (i=0; i<self->max; ++i) {
        struct client *c = &self->clients[i];
        if (c->connected) {
            if (self->query_first > 0) {
                for (n=0; n<self->query_first; ++n) {
                    send_one(self, c->connid);
                }
            } else {
                send_one(self, c->connid);
            }
        }
    }
}

int
benchmark_init(struct module* s) {
    struct benchmark* self = MODULE_SELF;

    self->query = sh_getint("benchmark_query", 0); 
    self->query_first = sh_getint("benchmark_query_first", 0);
    self->query_send = 0;
    self->query_recv = 0;
    self->query_done = 0;
    int sz = sh_getint("benchmark_packet_size", 10);
    if (sz < sizeof(struct UM_TEXT))
        sz = sizeof(struct UM_TEXT);
    self->packetsz = sz;
    self->packetsplit = sh_getint("benchmark_packet_split", 0);
    int hmax = sh_getint("sh_connmax", 0);
    int cmax = sh_getint("benchmark_client_max", 0); 
    
    self->max = cmax;
    self->clients = malloc(sizeof(struct client) * cmax);
    memset(self->clients, 0, sizeof(struct client) * cmax);
    freeid_init(&self->fi, cmax, hmax);
    
    self->start = 0;
    self->end = 0;

    self->connected = connect_clients(s);
    if (self->connected == 0) {
        return 1;
    }
    //start_send(self);
    sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static inline struct client*
get_client(struct benchmark* self, int id) {
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    assert(c->connected);
    return c;
}

static inline int
handle_msg(struct benchmark* self, struct client* c, const void *msg, int sz) {
    if (sz < sizeof(struct UM_BASE) || sz > UM_CLI_MAXSZ)
        return 1;
    self->query_done++;
    self->query_recv++;
    if (self->query_done == self->query) {
        self->end = sh_timer_now();
        uint64_t elapsed = self->end - self->start;
        if (elapsed == 0) elapsed = 1;
        float qps = self->query_done/(elapsed*0.001f);
        sh_info("clients: %d, packetsz: %d, query send: %d, recv: %d, done: %d, use time: %d, qps: %f", 
        self->connected, self->packetsz, self->query_send, self->query_recv, self->query_done, (int)elapsed, qps);
        self->start = self->end;
        self->query_done = 0;
    }
    send_one(self, c->connid);
    return 0;
}

static void
read_msg(struct benchmark* self, struct net_message* nm) {
    int id = nm->connid;
    struct client* c = get_client(self, id);
    assert(c);
    assert(c->connid == id);
    int step = 0;
    int drop = 1;
    int err;
    for (;;) {
        err = 0; 
        struct mread_buffer buf;
        int nread = sh_net_read(id, drop==0, &buf, &err);
        if (nread <= 0) {
            if (!err)
                return;
            else
                goto errout;
        }
        for (;;) {
            if (buf.sz < 2) {
                break;
            }
            uint16_t sz = sh_from_littleendian16((uint8_t*)buf.ptr) + 2;
            if (buf.sz < sz) {
                break;
            }
            if (handle_msg(self, c, buf.ptr+2, sz-2)) {
                err = NET_ERR_MSG;
                break;
            }
            buf.ptr += sz;
            buf.sz  -= sz;
            if (++step > 10) {
                sh_net_dropread(id, nread-buf.sz);
                return;
            }
        }
        if (err) {
            sh_net_close_socket(id, true);
            goto errout;
        }
        drop = nread - buf.sz;
        sh_net_dropread(id, drop);       
    }
    return;
errout:
    nm->type = NETE_SOCKERR;
    nm->error = err;
    module_net(nm->ud, nm);

}

static void
on_connect(struct benchmark* self, int connid) {
    int id = freeid_alloc(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    assert(!c->connected);
    c->connected = true;
    c->connid = connid;
    //c->active_time = sh_timer_now();
    
    sh_net_subscribe(connid, false);
}

static void
on_disconnect(struct benchmark* self, int connid) {
    int id = freeid_free(&self->fi, connid);
    if (id == -1) {
        return;
    }
    assert(id >= 0 && id < self->max);
    struct client* c = &self->clients[id];
    c->connected = false;
}

void
benchmark_net(struct module* s, struct net_message* nm) {
    struct benchmark* self = MODULE_SELF;
    switch (nm->type) {
    case NETE_READ:
        read_msg(self, nm);
        break;
    case NETE_CONNECT:
        on_connect(self, nm->connid);
        break;
    case NETE_SOCKERR:
        on_disconnect(self, nm->connid);
        break;
    }
}

void
benchmark_time(struct module* s) {
    struct benchmark* self = MODULE_SELF;
    if (self->query_send > 0) {
        return;
    }
    struct client* c = NULL;
    int n = 0;
    int i;
    for (i=0; i<self->max; ++i) {
        c = &self->clients[i];
        if (c->connected) {
            ++n;
        }
    }
    if (n != self->connected)
        return;

    start_send(self);
}
