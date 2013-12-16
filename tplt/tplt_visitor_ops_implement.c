#include "tplt_visitor_ops_implement.h"
#include "tplt_visitor_ops.h"
#include "tplt_visitor.h"
#include "tplt_holder.h"
#include <stdlib.h>
#include <string.h>

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
 * index32
 */
struct _index32 {
    int sz;
    void** p;
};

static int
_index32_create(struct tplt_visitor* visitor, struct tplt_holder* holder) {
    struct _index32* index = malloc(sizeof(*index));
     
    int i;
    uint32_t key, key_max = 0;
    char* ptr;

    ptr = holder->data;
    for (i=0; i<holder->nelem; ++i) {
        key = *(uint32_t*)ptr;
        if (key_max < key)
            key_max = key;
        ptr += holder->elemsz;
    }

    uint32_t max = key_max+1;
    void** p = malloc(sizeof(void*) * max);
    memset(p, 0, sizeof(*p));

    ptr = holder->data;
    for (i=0; i<holder->nelem; ++i) {
        key = *(uint32_t*)ptr;
        p[key] = ptr;
        ptr += holder->elemsz;
    }

    index->p = p;
    index->sz = max;
    visitor->data = index;
    return 0;
}

static void
_index32_free(struct tplt_visitor* visitor) {
    struct _index32* index = visitor->data;
    if (index) {
        free(index->p);
        free(index);
        visitor->data = NULL;
    }
}

static void*
_index32_find(const struct tplt_visitor* visitor, uint32_t key) {
    const struct _index32* index = visitor->data;
    if (key < index->sz) {
        return index->p[key];
    }
    return NULL;
}

const struct tplt_visitor_ops g_tplt_visitor_index32 = {
    _index32_create,
    _index32_free,
    _index32_find,
};
