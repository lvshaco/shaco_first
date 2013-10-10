#ifndef __gfreeid_h__
#define __gfreeid_h__

#include <stdlib.h>
#include <assert.h>

#define GFREEID_FIELDS(type) \
    int cap;            \
    struct type* p;     \
    struct type* freep; \

#define GFREEID_CAP(gfi) ((gfi)->cap)
#define GFREEID_FIRST(gfi) ((gfi)->p)
#define GFREEID_ID(s, gfi) ((s) - (gfi)->p)
#define GFREEID_USED(s) ((s)->used)
#define GFREEID_SLOT(gfi, id) \
    (((id)>=0 && (id)<(gfi)->cap) ? (((gfi)->p[id]).used ? (&(gfi)->p[id]) : NULL) : NULL)

#define _GFREEID_INITSLOTS(s, begin, end) do { \
    int i;                          \
    for (i=begin; i<end; ++i) {     \
        s[i].id = i+1;              \
        s[i].used = 0;              \
    }                               \
    s[end-1].id = -1;               \
} while (0);

#define GFREEID_ALLOC(type, gfi) ({ \
    if ((gfi)->freep == NULL) {                                 \
        assert((gfi)->cap > 0);                                 \
        int cap = (gfi)->cap * 2;                               \
        (gfi)->p = realloc((gfi)->p, sizeof(struct type) * cap);\
        _GFREEID_INITSLOTS((gfi)->p, (gfi)->cap, cap);          \
        (gfi)->freep = &(gfi)->p[(gfi)->cap];                   \
        (gfi)->cap = cap;                                       \
    }                                                           \
    struct type* free = (gfi)->freep;                           \
    if (free->id >= 0) {                                        \
        (gfi)->freep = &(gfi)->p[free->id];                     \
    } else {                                                    \
        (gfi)->freep = NULL;                                    \
    }                                                           \
    free->used = 1;                                             \
    free;                                                       \
})

#define GFREEID_FREE(type, gfi, s) do { \
    assert(s->used);                    \
    int id = s - (gfi)->p;              \
    assert(id >= 0);                    \
    if ((gfi)->freep) {                 \
        s->id = (gfi)->freep - (gfi)->p;\
    } else {                            \
        s->id = -1;                     \
    }                                   \
    s->used = 0;                        \
    (gfi)->freep = s;                   \
} while (0);

#define GFREEID_INIT(type, gfi, init) do { \
    int cap = 1;                                \
    while (cap < init)                          \
        cap *= 2;                               \
    (gfi)->p = malloc(sizeof(struct type) * cap); \
    (gfi)->freep = (gfi)->p;                    \
    (gfi)->cap = cap;                           \
    _GFREEID_INITSLOTS((gfi)->p, 0, cap);       \
} while (0);

#define GFREEID_FINI(type, gfi) do { \
    free((gfi)->p);       \
    (gfi)->p = NULL;      \
    (gfi)->freep = NULL;  \
    (gfi)->cap = 0;       \
} while (0);

#endif
