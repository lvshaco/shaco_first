#include "roommap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

struct readptr {
    char* bptr;
    char* rptr;
    int sz;
};

static inline int 
_rp_probe(struct readptr* rp, int sz) {
    if (rp->rptr - rp->bptr + sz <= rp->sz) {
        rp->rptr += sz;
        return 0;
    }
    return 1;
}

static inline int
_rp_read(struct readptr* rp, void* value, int sz) {
    if (rp->rptr - rp->bptr + sz <= rp->sz) {
        memcpy(value, rp->rptr, sz);
        rp->rptr += sz;
        return 0;
    }
    return 1;
}

static int 
_check(struct roommap* self, int sz) {
    int ncell = ROOMMAP_NCELL(self);
    int depth = ROOMMAP_DEPTH(self);
    int i;
    struct roommap_typeid_header th;
    uint8_t curoff = 0;
    uint8_t lastoff = curoff;

    struct readptr rp = { self->data, self->data, 
                          sz - sizeof(self->header) };
    for (i=0; i<depth; ++i) {
        if (_rp_read(&rp, &th, sizeof(th))) {
            return 1;
        }
        if (curoff != th.offset) {
            return 1;
        }
        curoff += th.num; 
        if (curoff < lastoff) {
            return 1; // overflow
        }
        lastoff = curoff;
    }

    if (_rp_probe(&rp, curoff*sizeof(struct roommap_typeid))) {
        return 1;
    }
    if (_rp_probe(&rp, ncell* sizeof(struct roommap_cell))) {
        return 1;
    }
    return 0;
}

#define MMAP_SIZE(sz) (offsetof(struct roommap, header) + (sz))

static int
_build(struct roommap* self) {
    struct roommap_typeid_header* th = (struct roommap_typeid_header*)self->data;
    uint16_t depth = ROOMMAP_DEPTH(self);
    struct roommap_typeid* ti = (struct roommap_typeid*)(th + depth);
    self->typeid_entry = ti;
    self->cell_entry = (struct roommap_cell*)(ti + th[depth].offset + th[depth].num);
    return 0;
}

struct roommap*
roommap_create(const char* file) {
    FILE* fp = fopen(file, "rb");

    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp); 

    struct roommap* self = (struct roommap*)malloc(MMAP_SIZE(fsize));
    memset(self, 0, sizeof(*self));
    fseek(fp, 0, SEEK_SET);
    if (fread(&self->header, fsize, 1, fp) != 1) {
        free(self);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    if (_check(self, fsize)) {
        free(self);
        return NULL;
    }
    if (_build(self)) {
        free(self);
        return NULL;
    }
    return self;
}

struct roommap*
roommap_createfromstream(void* stream, int sz) {
    if (sz <= sizeof(struct roommap_header)) {
        return NULL;
    }
    struct roommap* self = (struct roommap*)malloc(MMAP_SIZE(sz));
    memset(self, 0, sizeof(*self));
    memcpy(&self->header, stream, sz);
    if (_check(self, sz)) {
        free(self);
        return NULL;
    }
    if (_build(self)) {
        free(self);
        return NULL;
    }
    return self;
}

void
roommap_free(struct roommap* self) {
    free(self);
}
