#include "sc_reload.h"
#include "sc_init.h"
#include "sc_service.h"
#include "sh_util.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_MAX 10

struct reload_cache {
    int size;
    int services[CACHE_MAX];
};

static struct reload_cache* C = NULL;

int
sc_reload_prepare(const char* names) {
    char tmp[1024];
    sc_strncpy(tmp, names, sizeof(tmp));

    int sz = 0;
    int id;
    char* saveptr = NULL;
    char* one = strtok_r(tmp, ",", &saveptr);
    while (one) {
        id = service_query_id(one);
        if (id != -1) {
            C->services[sz++] = id;
        }
        one = strtok_r(NULL, ",", &saveptr);
    }
    C->size = sz;
    return sz;
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
