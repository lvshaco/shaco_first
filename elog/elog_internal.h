#ifndef __elog_internal_h__
#define __elog_internal_h__

struct elog_appender;

struct elog {
    const struct elog_appender* appender;
    char* filename;
    void* od;
};

#endif
