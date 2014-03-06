#ifndef __sh_array_h__
#define __sh_array_h__

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef int (*sh_array_each_t)(void *, void *);
typedef int (*sh_array_compare_t)(const void *, const void *);

struct sh_array {
    void  *elem;
    size_t nelem;
    size_t size;
    size_t cap;
};

struct sh_array *sh_array_new(size_t size, size_t cap);
void   sh_array_delete(struct sh_array *self);
int    sh_array_init(struct sh_array* self, size_t size, size_t cap);
void   sh_array_fini(struct sh_array* self);
int    sh_array_foreach(struct sh_array *self, sh_array_each_t func, void *ud);

//inline
//size_t sh_array_capacity(struct sh_array *self);
//size_t sh_array_size(struct sh_array *self);
//void * sh_array_get(struct sh_array *self, size_t idx);
//void * sh_array_push(struct sh_array *self);
//void * sh_array_pop(struct sh_array *self);
//void * sh_array_top(struct sh_array *self);
//void   sh_array_sort(struct sh_array *self, sh_array_compare_t func);

static inline size_t
sh_array_capacity(struct sh_array *self) {
    return self->cap;
}

static inline size_t
sh_array_n(struct sh_array *self) {
    return self->nelem;
}

static inline void *
sh_array_get(struct sh_array *self, size_t idx) {
    assert(idx < self->nelem);
    return (uint8_t*)self->elem + self->size * idx;
}

static inline void *
sh_array_top(struct sh_array *self) {
    return sh_array_get(self, self->nelem - 1);
}

static inline void *
sh_array_pop(struct sh_array *self) {
    assert(self->nelem > 0);
    self->nelem --;
    return (uint8_t*)self->elem + self->size * self->nelem;
}

static inline void *
sh_array_push(struct sh_array *self) {
    if (self->nelem == self->cap) {
        self->cap *= 2;
        self->elem = realloc(self->elem, self->size * self->cap);
    }
    void *elem = (uint8_t*)self->elem + self->size * self->nelem;
    self->nelem ++;
    return elem;
}

static inline void
sh_array_sort(struct sh_array *self, sh_array_compare_t func) {
    qsort(self->elem, self->nelem, self->size, func);
}

#endif
