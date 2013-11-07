#ifndef __elog_ops_h__
#define __elog_ops_h__

struct elog;
struct elog_ops {
    int  (*init)(struct elog* self);
    void (*fini)(struct elog* self);
    int  (*append)(struct elog* self, const char* msg, int sz);
};

#endif
