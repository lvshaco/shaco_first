#include "sh_init.h"
#include "sh_module.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_MAX 30

struct reload_cache {
    int cap;
    int sz;
    int *p;
};

static struct reload_cache* C = NULL;

static inline void
cache(int id) {
    if (C->sz == C->cap) {
        C->cap *= 2;
        if (C->cap == 0)
            C->cap = 1;
        C->p = realloc(C->p, sizeof(C->p[0]) * C->cap);
    }
    C->p[C->sz++] = id;
}

int
sh_reload_prepare(const char* mods) {
    int l = strlen(mods);
    if (l >= 1024)
        return 0;
    char tmp[l+1];
    strcpy(tmp, mods);
    
    int id;
    char *save = NULL, *one;
    one = strtok_r(tmp, ",", &save);
    while (one) {
        if (C->sz < CACHE_MAX) {
            id = module_query_id(one);
            if (id != -1) {
                cache(id);
            }
        }
        one = strtok_r(NULL, ",", &save);
    }
    return C->sz;
}

void
sh_reload_execute() {
    if (C->sz <= 0) {
        return;
    }
    int i;
    for (i=0; i<C->sz; ++i) {
        module_reload_byid(C->p[i]);
    }
    C->sz = 0;
}

static void
sh_reload_init() {
    C = malloc(sizeof(*C));
    memset(C, 0, sizeof(*C));
}

static void
sh_reload_fini() {
    if (C == NULL)
        return;
    if (C->p) {
        free(C->p);
        C->p = NULL;
    }
    free(C);
    C = NULL;
}

SH_LIBRARY_INIT_PRIO(sh_reload_init, sh_reload_fini, 0);
