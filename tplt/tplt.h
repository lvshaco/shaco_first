#ifndef __tplt_h__
#define __tplt_h__

#include <stdint.h>

struct tplt;
struct tplt_holder;
struct tplt_visitor;

struct tplt_desc {
    int type; // see TPLT_*
    int size; // sizeof(*_tplt)
    int isfromfile;
    const char* stream; // filename if is fromfile, or streamptr
    int streamsz;
    const struct tplt_visitor_ops* vist;
};

struct tplt* tplt_create(const struct tplt_desc* desc, int sz);
void tplt_free(struct tplt *self);
const struct tplt_holder* tplt_get_holder(struct tplt *self, int type);
const struct tplt_visitor* tplt_get_visitor(struct tplt *self, int type);
void* tplt_find(struct tplt *self, int type, uint32_t key);

#endif
