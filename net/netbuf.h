#ifndef __NETBUF_H__
#define __NETBUF_H__

#include <stdint.h>

struct netbuf_block {
    int sz;
    int rptr;
    int wptr;
};

#define RB_RPTR(rb) ((uint8_t*)((rb)+1) + (rb)->rptr)
#define RB_WPTR(rb) ((uint8_t*)((rb)+1) + (rb)->wptr)
#define RB_SPACE(rb) ((rb)->sz   - (rb)->wptr)
#define RB_NREAD(rb) ((rb)->wptr - (rb)->rptr)

struct netbuf;

struct netbuf_block* netbuf_alloc_block(struct netbuf* self, int id);
void netbuf_free_block(struct netbuf* self, struct netbuf_block* block);

struct netbuf* netbuf_create(int max, int block_size);
void netbuf_free(struct netbuf* self);

#endif
