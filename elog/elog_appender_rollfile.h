#ifndef __elog_appender_rollfile_h__
#define __elog_appender_rollfile_h__

#include "elog_appender.h"

extern const struct elog_appender g_elog_appender_rollfile;

struct elog_rollfile_conf {
    int  file_max_num;
    long file_max_size;
};

int elog_appender_rollfile_config(struct elog* self, 
        const struct elog_rollfile_conf* conf);

#endif
