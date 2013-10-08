#ifndef __gfreeid_h__
#define __gfreeid_h__

#define gfreeid_declare(type, name) \
struct name {           \
    int cap;            \
    struct type* p;     \
    struct type* freep; \
};

#define _gfreeid_initslots(s, begin, end) do { \
    int i;                          \
    for (i=begin; i<end-1; ++i) {   \
        s[i].id = i+1;              \
        s[i].used = 0;              \
    }                               \
    s[i].id = -1;                   \
} while (0);

#define gfreeid_alloc(type, gfi) do { \
_alloc_again:                                               \
    struct type* free = gfi->freep;                         \
    if (free) {                                             \
        if (free->id >= 0) {                                \
            gfi->freep = &gfi->p[free->id];                 \
        } else {                                            \
            gfi->freep = NULL;                              \
        }                                                   \
        free->used = 1;                                     \
        return free;                                        \
    } else {                                                \
        assert(gfi->cap > 0);                               \
        int cap = gfi->cap * 2;                             \
        gfi->p = realloc(gfi->p, sizeof(struct type) * cap);\
        _gfreeid_initslots(gfi->p, gfi->cap, cap);          \
        gfi->freep = &gfi->p[gfi->cap];                     \
        gfi->cap = cap;                                     \
        goto _alloc_again;                                  \
    }                                                       \
} while (0);

#define gfreeid_free(type, gfi, s) do { \
    int id = s - gfi->p;            \
    assert(id >= 0);                \
    if (gfi->freep) {               \
        s->id = gfi->freep - gfi->p;\
    } else {                        \
        s->id = -1;                 \
    }                               \
    s->used = 0;                    \
    gfi->freep = s;                 \
} while (0);

#define gfreeid_create(type, gfi, init) do { \
    int cap = 1;                                \
    while (cap < init)                          \
        cap *= 2;                               \
    gfi->p = malloc(sizeof(struct type) * cap); \
    gfi->freep = gfi->p;                        \
    gfi->cap = cap;                             \
    _gfreeid_initslots(gfi->p, 0, cap);         \
} while (0);

#define gfreeid_destroy(type, gfi) do { \
    free(gfi->p);       \
    gfi->p = NULL;      \
    gfi->freep = NULL;  \
    gfi->cap = 0;       \
} while (0);

#endif
