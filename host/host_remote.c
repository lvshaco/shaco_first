#include "host_remote.h"
#include <stdlib.h>

#define REMOTE_MAX 0xff

struct remote {
    int id;
    int connid;
    int sign; 
    void* data;
    size_t sz;
};

struct remote_holder {
    struct remote* nodes[REMOTE_MAX];
};

static remote_holder* R = NULL;

void 
remote_init() {
    R = malloc(sizeof(*R));
    memset(R, 0, sizeof(*R));
    return R;
}

void 
remote_fini() {
    free(R);
}

int
_conn2remote(int connid) {
    int i;
    for (i=0; i<REMOTE_MAX; ++i) {
        if (R->nodes[i] &&
            R->nodes[i].connid == connid) {
            return i;
        }
    }
    return -1;
}

int
remote_create(int connid) {
    struct remote* free = NULL;
    int i;
    for (i=0; i<REMOTE_MAX; ++i) {
        if (R->nodes[i] == NULL) {
            free = malloc(sizeof(struct remote));
            R->nodes[i] = free;
        }
    }
    if (free) {
        free->id = i;
        free->connid = connid;
        free->sign = 0;
        free->data = NULL;
        free->sz = 0;
    }
    return free;
}

int
remote_destroy(int connid) {
    int id = _conn2remote(connid);
    if (id >= 0) {
        free(R->nodes[id]);
        R->nodes[id] = NULL;
    }
    return id;
}

void
remote_handle(int connid, struct host_message* hmsg) {
    int id = _conn2remote(connid);
    if (id < 0) {
        host_error("remote handle error : unknown connid %d", connid);
        return;
    }
    // reg
    // unreg
}

void 
remote_multicast(int sign, struct host_message* hmsg) {
    int i;
    for (i=0; i<REMOTE_MAX; ++i) {
        if (R->nodes[i] &&
            R->nodes[i].sign == sign) {
            // connection send
        }
    }
}

void 
remote_send(int id, struct host_message* hmsg) {
    assert(id >= 0 && id < REMOTE_MAX);
    struct remote* re = R->nodes[id];
    if (re) {
        // error: disconnected ?
        return;
    }
    // connection send
}
