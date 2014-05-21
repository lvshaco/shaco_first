#ifndef __elog_appender_h__
#define __elog_appender_h__

struct elog;
struct elog_appender {
    int  (*open)(struct elog* self, const char *mode);
    void (*close)(struct elog* self);
    void (*append)(struct elog* self, const char* msg, int sz);
};

#endif
