#include "sh_array.h"

struct sh_array *
sh_array_new(size_t size, size_t cap) {
    struct sh_array *self = malloc(sizeof(*self));
    if (sh_array_init(self, size, cap)) {
        free(self);
        return NULL;
    } else
        return self;
}

void
sh_array_delete(struct sh_array *self) {
    if (self == NULL)
        return;
    sh_array_fini(self);
    free(self);
}

int    
sh_array_init(struct sh_array* self, size_t size, size_t cap) {
    assert(size != 0);

    int tmp = 1;
    while (tmp < cap) {
        tmp *= 2;
    }
    self->size = size;
    self->nelem = 0;
    self->cap = tmp;
    self->elem = malloc(self->size * self->cap);
    return 0;
}

void   
sh_array_fini(struct sh_array* self) {
    if (self->elem == NULL)
        return;
    free(self->elem);
    self->elem = NULL;
}

int
sh_array_foreach(struct sh_array *self, sh_array_each_t func, void *ud) {
    size_t i;
    for (i=0; i<self->nelem; ++i) {
        void *elem = sh_array_get(self, i);
        int status = func(elem, ud);
        if (status) {
            return status;
        }
    }
    return 0;
}
