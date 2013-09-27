#include "netbuf.h"
#include <stdlib.h>
#include <assert.h>

struct netbuf {
    int max;
    int block_size;
    char blocks[0];
};

struct netbuf_block* 
netbuf_alloc_block(struct netbuf* self, int id) {
    assert(id >= 0 && id < self->max);
    struct netbuf_block* buf_b =  (void*)self->blocks + id * self->block_size;
    buf_b->sz = self->block_size - sizeof(*buf_b);
    buf_b->rptr = 0;
    buf_b->wptr = 0;
    return buf_b;
}

void 
netbuf_free_block(struct netbuf* self, struct netbuf_block* block) {
    block->rptr = 0;
    block->wptr = 0;
}

struct netbuf* 
netbuf_create(int max, int block_size) {
    if (max == 0 || block_size == 0)
        return NULL;
    struct netbuf* nb = malloc(sizeof(struct netbuf) + max * block_size);
    nb->max = max;
    nb->block_size = block_size;
    return nb;
}

void 
netbuf_free(struct netbuf* self) {
    free(self);
}
