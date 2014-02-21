#ifndef __buff_h__
#define __buff_h__

#include <stdlib.h>
#include <string.h>

struct buff_delay {
    uint32_t id;
    uint64_t first_effect_time;
    uint64_t last_effect_time;
};

struct one_effect {
    int  type;
    bool isper;
    float value;
};

#define BUFF_EFFECT 3
struct buff_effect {
    uint32_t id;
    struct one_effect effects[BUFF_EFFECT];
    uint64_t time;
};

struct delay_vector {
    int cap;
    int sz;
    struct buff_delay *p;
};

struct effect_vector {
    int cap;
    int sz;
    struct buff_effect *p;
};

// buff delay
static struct buff_delay *
delay_find(struct delay_vector *D, uint32_t id) {
    int i;
    for (i=0; i<D->sz; ++i) {
        if (D->p[i].id == id)
            return &D->p[i];
    }
    return NULL;
}

static inline void
delay_del(struct buff_delay *one) {
    one->id = 0;
}

static struct buff_delay *
delay_add(struct delay_vector *D, uint32_t id) {
    int i, sz = D->sz;
    for (i=0; i<sz; ++i) {
        if (D->p[i].id == 0) {
            D->p[i].id = id;
            return &D->p[i];
        }
    }
    if (sz >= D->cap) {
        D->cap *= 2;
        D->p = realloc(D->p, sizeof(D->p[0]) * D->cap);
    }
    D->p[sz].id = id;
    D->sz++;
    return &D->p[sz];
}

static inline void
delay_init(struct delay_vector *D) {
    D->sz  = 0;
    D->cap = 1;
    D->p = malloc(sizeof(D->p[0]) * D->cap);
}

static void
delay_fini(struct delay_vector *D) {
    if (D == NULL) return;
    free(D->p);
    D->p = NULL;
}

// buff effect
static struct buff_effect *
effect_find(struct effect_vector *E, uint32_t id) {
    int i;
    for (i=0; i<E->sz; ++i) {
        if (E->p[i].id == id)
            return &E->p[i];
    }
    return NULL;
}

static inline void
effect_del(struct buff_effect *one) {
    one->id = 0;
}

static struct buff_effect *
effect_add(struct effect_vector *E, uint32_t id) {
    int i, sz = E->sz;
    for (i=0; i<sz; ++i) {
        if (E->p[i].id == 0) {
            E->p[i].id = id;
            return &E->p[i];
        }
    }
    if (sz >= E->cap) {
        E->cap *= 2;
        E->p = realloc(E->p, sizeof(E->p[0]) * E->cap);
    }
    E->p[sz].id = id;
    E->sz++;
    return &E->p[sz];
}

static inline void
effect_init(struct effect_vector *E) {
    E->sz  = 0;
    E->cap = 1;
    E->p = malloc(sizeof(E->p[0]) * E->cap);
}

static inline void
effect_fini(struct effect_vector *E) {
    if (E == NULL) return;
    free(E->p);
    E->p = NULL;
}

#endif
