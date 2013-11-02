#ifndef __tplt_visitor_ops_h__
#define __tplt_visitor_ops_h__

#include <stdint.h>

struct tplt_holder;
struct tplt_visitor;

struct tplt_visitor_ops {
    int   (*create)(struct tplt_visitor* visitor, struct tplt_holder* holder);
    void  (*free)(struct tplt_visitor* visitor);
    void* (*find)(const struct tplt_visitor* visitor, uint32_t key);
};

#endif
