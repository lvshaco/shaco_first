#include "sh_reload.h"
#include "sh_init.h"
#include "sh_module.h"
#include "sh_util.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_MAX 10

struct reload_cache {
    int size;
    int modules[CACHE_MAX];
};

static struct reload_cache* C = NULL;

int
sh_reload_prepare(const char* names) {
    char tmp[1024];
    sh_strncpy(tmp, names, sizeof(tmp));

    int sz = 0;
    int id;
    char* saveptr = NULL;
    char* one = strtok_r(tmp, ",", &saveptr);
    while (one) {
        id = module_query_id(one);
        if (id != -1) {
            C->modules[sz++] = id;
        }
        one = strtok_r(NULL, ",", &saveptr);
    }
    C->size = sz;
    return sz;
}

void
sh_reload_execute() {
    if (C->size <= 0)
        return;

    int i;
    for (i=0; i<C->size; ++i) {
        module_reload_byid(C->modules[i]);
    }
    C->size = 0;
}

static void
sh_reload_init() {
    C = malloc(sizeof(*C));
    C->size = 0;
}

static void
sh_reload_fini() {
    free(C);
    C = NULL;
}

SH_LIBRARY_INIT_PRIO(sh_reload_init, sh_reload_fini, 0);
