#ifndef __tplt_visitor_h__
#define __tplt_visitor_h__

#include <stdint.h>

struct tplt_holder;
struct tplt_visitor_ops;

struct tplt_visitor {
    const struct tplt_visitor_ops* ops;
    void* data;
};

struct tplt_visitor* tplt_visitor_create(const struct tplt_visitor_ops* ops, 
                                         struct tplt_holder* holder);
void tplt_visitor_free(struct tplt_visitor* visitor);
void* tplt_visitor_find(const struct tplt_visitor* visitor, uint32_t key);

#endif
