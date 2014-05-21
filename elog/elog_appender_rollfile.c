#include "elog_appender_rollfile.h"
#include "elog_internal.h"
#include "elog.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

struct appender_data {
    struct elog_rollfile_conf conf;
    FILE* current_fp;
    long current_size;
    char** filenames;
};

static int
_get_last_fileindex(struct appender_data* od) {
    struct stat info;
    int i = 0;
    while (i < od->conf.file_max_num &&
            stat(od->filenames[i], &info) == 0) {
        i++;
    }
    return i;
}

static void
_set_filemaxnum(struct elog* self, struct appender_data* od, int max) {
    if (max < 1)
        return;
    if (max <= od->conf.file_max_num)
        return;
    od->filenames = realloc(od->filenames, sizeof(char*) * max);

    int len = strlen(self->filename) + 10 + 2;
    int i;
    for (i=od->conf.file_max_num; i<max; ++i) {
        od->filenames[i] = malloc(len);
        if (i == 0)
            snprintf(od->filenames[i], len, "%s", self->filename);
        else
            snprintf(od->filenames[i], len, "%s.%d", self->filename, i);
    }
    od->conf.file_max_num = max;
}

static void
_rollover(struct appender_data* od) {
    assert(od->current_fp == NULL);

    int last = _get_last_fileindex(od);

    if (last == od->conf.file_max_num) {
        unlink(od->filenames[last-1]);
        last--;
    }
    int i;
    for (i=last; i>0; --i) {
        rename(od->filenames[i-1], od->filenames[i]);
    }

    const char* filename = od->filenames[0];
    FILE* fp = fopen(filename, "w+");
    if (fp == NULL) {
        fp = stderr;
        printf("open zero file: stderr\n");
    } else {
        printf("open zero file: %s\n", filename);
    }
    setbuf(fp, NULL);
    od->current_fp = fp;
    od->current_size = 0; 
    
}

static int
elog_rollfile_open(struct elog* self, const char *mode) {
    FILE* fp = fopen(self->filename, mode);
    if (fp == NULL) {
        return 1;
    }
    setbuf(fp, NULL);

    struct appender_data* od = malloc(sizeof(*od));
    memset(od, 0, sizeof(*od));

    od->conf.file_max_size = 1024*1024*1024; // default 1G
    _set_filemaxnum(self, od, 10);  // default 10 files

    fseek(fp, 0, SEEK_END);
    long pos = ftell(fp);
    od->current_fp = fp;
    od->current_size = pos;
    
    self->od = od;
    return 0;
}

static void
elog_rollfile_close(struct elog* self) {
    int i;
    struct appender_data* od = self->od;
    if (od == NULL)
        return;
   
    if (od->current_fp) {
        fclose(od->current_fp);
        od->current_fp = NULL;
        od->current_size = 0;
    }
    if (od->filenames) {
        for (i=0; i<od->conf.file_max_num; ++i) {
            free(od->filenames[i]);
        }
        free(od->filenames);
    }
    free(od);
    self->od = NULL;
}

static void
elog_rollfile_append(struct elog* self, const char* msg, int sz) {
    struct appender_data* od = self->od;
    if (od->current_size >= od->conf.file_max_size) {
        fclose(od->current_fp);
        od->current_fp = NULL;
        od->current_size = 0;
        _rollover(od);
    }
    fwrite(msg, sz, 1, od->current_fp);
    od->current_size += sz;
}

const struct elog_appender g_elog_appender_rollfile = {
    elog_rollfile_open,
    elog_rollfile_close,
    elog_rollfile_append,
};

int 
elog_appender_rollfile_config(struct elog* self, const struct elog_rollfile_conf* conf) {
    if (self->appender != &g_elog_appender_rollfile)
        return 1;

    struct appender_data* od = self->od;
    _set_filemaxnum(self, od, conf->file_max_num);
    od->conf.file_max_size = conf->file_max_size;
    return 0;
}
