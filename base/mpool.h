#ifndef __MPOOL_H__
#define __MPOOL_H__

#include <stdlib.h>

struct mpool;
struct mpool* mpool_new(size_t page_size);
void   mpool_delete(struct mpool* m);
void*  mpool_alloc(struct mpool* m, size_t n);
void*  mpool_realloc(struct mpool* m, void* p, size_t n);
void   mpool_dump(struct mpool* m);

#endif
