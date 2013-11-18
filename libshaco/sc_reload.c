#include "sc_reload.h"
#include "sc_init.h"
#include "sc_service.h"
#include "args.h"
#include <stdlib.h>

#define CACHE_MAX 10

struct reload_cache {
    int size;
    int services[CACHE_MAX];
};

static struct reload_cache* C = NULL;

void
sc_reload_prepare(const char* names) {
    struct args A;
    args_parsestr(&A, CACHE_MAX, names);
    if (A.argc == 0)
        return;
    int sz = 0;
    int id;
    int i;
    for (i=0; i<A.argc; ++i) {
        id = service_query_id(A.argv[i]);
        if (id != SERVICE_INVALID) {
            C->services[sz++] = id;
        }
    }
    C->size = sz;
}

void
sc_reload_execute() {
    if (C->size <= 0)
        return;

    int i;
    for (i=0; i<C->size; ++i) {
        service_reload_byid(C->services[i]);
    }
    C->size = 0;
}

static void
sc_reload_init() {
    C = malloc(sizeof(*C));
    C->size = 0;
}

static void
sc_reload_fini() {
    free(C);
    C = NULL;
}

SC_LIBRARY_INIT_PRIO(sc_reload_init, sc_reload_fini, 0);
