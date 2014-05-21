#include "elog_appender_file.h"
#include "elog_internal.h"
#include "elog.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct appender_data {
    FILE* fp;
};

static int
elog_file_open(struct elog* self, const char *mode) {
    FILE* fp;
    if (self->filename[0]) {
        fp = fopen(self->filename, mode);
        if (fp == NULL) {
            fprintf(stderr, "open log file fail %d: %s\n", errno, self->filename);
            return 1;
        }
    } else {
        fp = stderr;
    }
    struct appender_data* od = malloc(sizeof(*od));
    od->fp = fp;
    setbuf(od->fp, NULL);
    
    self->od = od;
    return 0;
}

static void
elog_file_close(struct elog* self) {
    struct appender_data* od = self->od;
    if (od == NULL) {
        return;
    }
    if (od->fp &&
        od->fp != stderr) {
        fclose(od->fp);
        od->fp = NULL;
    }
    free(od);
    self->od = NULL;
}

static void
elog_file_append(struct elog* self, const char* msg, int sz) {
    struct appender_data* od = self->od;
    fwrite(msg, sz, 1, od->fp);
}

const struct elog_appender g_elog_appender_file = {
    elog_file_open,
    elog_file_close,
    elog_file_append,
};
