#include "elog_ops_file.h"
#include "elog.h"
#include <stdio.h>

struct udata {
    FILE* file_current_fp;
    char* file_base_name;
};

static int
elog_file_init(struct elog* self) {
    //struct udata* ud = malloc(
    return 0;
}

static void
elog_file_fini(struct elog* self) {
}

static int
elog_file_append(struct elog* self, const char* msg, int sz) {
    return 0;
}

const struct elog_ops g_elog_ops_file = {
    elog_file_init,
    elog_file_fini,
    elog_file_append
};
