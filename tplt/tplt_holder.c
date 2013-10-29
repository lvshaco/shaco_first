#include "tplt_holder.h"
#include "tplt_internal.h"
#include <stdio.h>
#include <stdlib.h>

struct tplt_holder* 
tplt_holder_load(const char* file, int elemsz) {
    FILE* fp = fopen(file, "rb");
    if (fp == NULL) {
        TPLT_LOGERR("open fail");
        return NULL;
    }
    
    int32_t nelem = 0; 
    if (fread(&nelem, sizeof(nelem), 1, fp) != 1) {
        TPLT_LOGERR("read row count fail");
        fclose(fp);
        return NULL;
    }

    int32_t rowsz = 0;
    if (fread(&rowsz, sizeof(rowsz), 1, fp) != 1) {
        TPLT_LOGERR("read rowsz fail");
        fclose(fp);
        return NULL;
    }
    if (rowsz != elemsz) {
        TPLT_LOGERR("rowsz dismatch (tbl#%u, c#%u)", rowsz, elemsz);
        fclose(fp);
        return NULL;
    }

    int pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);

    int needfsize = elemsz * nelem + sizeof(nelem) + sizeof(rowsz);
    if (fsize != needfsize) {
        TPLT_LOGERR("file nelem dismatch (tbl#%d, c#%d)", fsize, needfsize);
        fclose(fp);
        return NULL;
    }
    fseek(fp, pos, SEEK_SET);

    struct tplt_holder* t = (struct tplt_holder*)malloc
        (sizeof(struct tplt_holder) + elemsz * nelem);
    t->nelem = nelem;
    t->elemsz = elemsz;

    char* ptr = t->data;
    int i;
    for (i=0; i<nelem; ++i) {
        if (fread(ptr, elemsz, 1, fp) != 1) {
            TPLT_LOGERR("read row error");
            free(t);
            fclose(fp);
            return NULL;
        }
        ptr += elemsz;
    }
    fclose(fp);
    return t;
}

void 
tplt_holder_free(struct tplt_holder* self) {
    free(self);
}
