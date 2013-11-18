#include "sc_init.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct sc_library {
    bool inited;
    struct sc_library_entry* p;
    int cap;
    int sz;
};

static struct sc_library* L = NULL;

void
sc_library_entry_register(struct sc_library_entry* entry) {
    if (L == NULL) {
        L = malloc(sizeof(*L));
        memset(L, 0, sizeof(*L));
    }
    if (L->inited) {
        return;
    }
    if (L->sz >= L->cap) {
        L->cap *= 2;
        if (L->cap == 0)
            L->cap = 1;
        L->p = realloc(L->p, sizeof(struct sc_library_entry) * L->cap);
    }
    L->p[L->sz] = *entry;
    L->sz++;
}

static int 
_compare_library_init_entry(const void* p1, const void* p2) {
    const struct sc_library_entry* e1 = p1;
    const struct sc_library_entry* e2 = p2;
    int diff;
    diff = e1->prio - e2->prio;
    if (diff)
        return diff;
    diff = strcmp(e1->filename, e2->filename);
    if (diff)
        return diff;
    diff = e1->fileline - e2->fileline;
    if (diff)
        return diff;
    return (intptr_t)e1 - (intptr_t)e2;
}

static void
sc_library_fini() {
    if (L == NULL)
        return;
    struct sc_library_entry* entry;
    int i;
    for (i=L->sz-1; i>=0; --i) {
        entry = &L->p[i];
        if (entry->fini) {
            entry->fini();
        }
    }
    free(L->p);
    free(L);
}

void
sc_init() {
    if (L == NULL)
        return;

    L->inited = true;

    qsort(L->p, L->sz, sizeof(L->p[0]), _compare_library_init_entry);
    struct sc_library_entry* entry;
    int i;
    for (i=0; i<L->sz; ++i) {
        entry = &L->p[i];
        if (entry->init) {
            entry->init();
        }
    }
    atexit(sc_library_fini);
}
