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

int tplt_init(const struct tplt_desc* desc, int sz);
void tplt_fini();
const struct tplt_holder* tplt_get_holder(int type);
const struct tplt_visitor* tplt_get_visitor(int type);
void* tplt_find(int type, uint32_t key);

#endif
