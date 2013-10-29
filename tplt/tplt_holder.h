#ifndef __tplt_holder_h__
#define __tplt_holder_h__

#include <stdint.h>

struct tplt_holder {
    int elemsz;
    int nelem;
    char data[];
};

/* 
 @ visit element sample
 struct type* data;
 int i;
 data = TPLT_HOLDER_FIRSTELEM(type, holder);
 for (i=0; i<TPLT_HOLDER_NELEM(holder); ++i) {
     data[i];
 }
*/

#define TPLT_HOLDER_NELEM(holder) ((holder)->nelem)
#define TPLT_HOLDER_FIRSTELEM(type, holder) ((struct type*)((holder)->data))

struct tplt_holder* tplt_holder_load(const char* file, int elemsz);
void tplt_holder_free(struct tplt_holder* self);

#endif
