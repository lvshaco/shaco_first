#include "tplt_holder.h"
#include "tplt_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
struct readptr {
    const char* ptr;
    const char* rptr;
    int sz;
};

static inline int
_read(struct readptr* rp, void* value, int sz) {
    if (rp->rptr - rp->ptr + sz <= rp->sz) {
        memcpy(value, rp->rptr, sz);
        rp->rptr += sz;
        return 0;
    }
    return 1;
}
*/

static int
_check(struct tplt_holder* self, int streamsz, int elemsz) {
    if (self->nelem < 0) {
        TPLT_LOGERR("row count invalid, must > 0");
        return 1;
    }
    if (self->elemsz != elemsz) {
        TPLT_LOGERR("rowsz dismatch (tbl#%u, c#%u)", self->elemsz, elemsz);
        return 1;
    }
    int needsz = sizeof(struct tplt_holder) + self->elemsz * self->nelem;
    if (needsz != streamsz) {
        TPLT_LOGERR("file nelem dismatch (tbl#%d, c#%d)", streamsz , needsz);
        return 1;
    }
    return 0;
}

struct tplt_holder* 
tplt_holder_load(const char* file, int elemsz) {
    FILE* fp = fopen(file, "rb");
    if (fp == NULL) {
        TPLT_LOGERR("open fail");
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp); 

    struct tplt_holder* self = (struct tplt_holder*)malloc(fsize);

    fseek(fp, 0, SEEK_SET);
    if (fread(self, fsize, 1, fp) != 1) {
        TPLT_LOGERR("read fail");
        free(self);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    
    if (_check(self, fsize, elemsz)) {
        free(self);
        return NULL;
    }
    return self; 
}

struct tplt_holder*
tplt_holder_loadfromstream(const void* stream, int streamsz, int elemsz) {
    if (streamsz < sizeof(struct tplt_holder)) {
        TPLT_LOGERR("read tbl head fail");
        return NULL;
    }
    struct tplt_holder* self = (struct tplt_holder*)malloc(streamsz);
    memcpy(self, stream, streamsz);

    if (_check(self, streamsz, elemsz)) {
        free(self);
        return NULL;
    }
    return self;
}

void 
tplt_holder_free(struct tplt_holder* self) {
    free(self);
}
