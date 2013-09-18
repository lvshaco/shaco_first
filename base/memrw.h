#ifndef __memrw_h__
#define __memrw_h__

#include <string.h>

struct memrw {
    char* begin;
    char* ptr;
    int sz;
};

#define RW_SPACE(rw) ((rw)->sz - ((rw)->ptr - (rw)->begin))
#define RW_EMPTY(rw) ((rw)->ptr == (rw)->begin)
#define RW_CUR(rw) ((rw)->ptr - (rw)->begin)

static inline void
memrw_init(struct memrw* rw, void* data, size_t sz) {
    rw->begin = data;
    rw->ptr = data;
    rw->sz = sz;
};

static inline int
memrw_write(struct memrw* rw, void* data, size_t sz) {
    int space = rw->sz - (rw->ptr - rw->begin);
    if (space >= (int)sz) {
        memcpy(rw->ptr, data, sz);
        rw->ptr += sz;
        return sz;
    }
    return -1;
}

static inline int
memrw_read(struct memrw* rw, void* data, size_t sz) {
    int space = rw->sz - (rw->ptr - rw->begin);
    if (space >= (int)sz) {
        memcpy(data, rw->ptr, sz);
        rw->ptr += sz;
        return sz;
    }
    return -1;
}

static inline int
memrw_pos(struct memrw* rw, size_t sz) {
    int space = rw->sz - (rw->ptr - rw->begin);
    if (space >= (int)sz) {
        rw->ptr += sz;
        return sz;
    }
    return -1;
}

#endif
