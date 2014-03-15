#include "room_map.h"
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
    self->depth = (self->header.height+99)/100;
    if (self->depth <= 0)
        return 1;

    int ncell = ROOMMAP_NCELL(self);
    int i;
    struct roommap_typeid_header th;
    uint8_t curoff = 0;
    uint8_t lastoff = curoff;

    struct readptr rp = { self->data, self->data, 
                          sz - sizeof(self->header) };
    for (i=0; i<self->depth; ++i) {
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
    uint16_t depth = ROOMMAP_DEPTH(self);
    if (depth <= 0)
        return 1;

    struct roommap_typeid_header* th = (struct roommap_typeid_header*)self->data; 
    struct roommap_typeid* ti = (struct roommap_typeid*)(th + depth);
    self->typeid_entry = ti;

    int off = depth * sizeof(struct roommap_typeid_header);
    off += (th[depth-1].offset + th[depth-1].num) * sizeof(struct roommap_typeid);
    int pack_off = (off+3)/4*4;

    //void *p1 = (char*)self->data + off;
    //void *p2 = (char*)self->data + pack_off;
    //void *p3 = (struct roommap_cell*)(ti + th[depth-1].offset + th[depth-1].num);
    //sh_info("pack_off %d, p1 %p, p2 %p, p3 %p", pack_off, p1, p2, p3);
    //self->cell_entry = (struct roommap_cell*)(ti + th[depth-1].offset + th[depth-1].num);
    self->cell_entry = (struct roommap_cell*)((char*)self->data + pack_off);
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
