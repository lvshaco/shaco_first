#include "tplt_visitor_ops_implement.h"
#include "tplt_visitor_ops.h"
#include "tplt_visitor.h"
#include "tplt_holder.h"
#include <stdlib.h>

/*
 * vec32
 */
struct _vec32_entry {
    uint32_t key;
    void* valptr;
};

struct _vec32 {
    int sz;
    struct _vec32_entry* p;
};

static int
_vec32_create(struct tplt_visitor* visitor, struct tplt_holder* holder) {
    struct _vec32* vec = malloc(sizeof(*vec));
    struct _vec32_entry* p = malloc(sizeof(struct _vec32_entry) * holder->nelem);
    char* ptr = holder->data;
    int i;
    for (i=0; i<holder->nelem; ++i) {
        p[i].key = *(uint32_t*)ptr;
        p[i].valptr = ptr;
        ptr += holder->elemsz;
    }
    vec->p = p;
    vec->sz = holder->nelem;
    visitor->data = vec;
    return 0;
}

static void
_vec32_free(struct tplt_visitor* visitor) {
    struct _vec32* vec = visitor->data;
    if (vec) {
        free(vec->p);
        free(vec);
        visitor->data = NULL;
    }
}

static void*
_vec32_find(const struct tplt_visitor* visitor, uint32_t key) {
    const struct _vec32* vec = visitor->data;
    int i;
    for (i=0; i<vec->sz; ++i) {
        if (key == vec->p[i].key) {
            return vec->p[i].valptr;
        }
    }
    return NULL;
}

const struct tplt_visitor_ops g_tplt_visitor_vec32 = {
    _vec32_create,
    _vec32_free,
    _vec32_find,
};

/*
 * hash32
 */
