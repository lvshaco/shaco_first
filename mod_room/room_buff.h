#ifndef __room_buff_h__
#define __room_buff_h__

#include "room.h"
#include "sh_array.h"

// buff delay
static struct buff_delay *
buff_delay_find(struct sh_array *a, uint32_t id) {
    int i;
    for (i=0; i<a->nelem; ++i) {
        struct buff_delay *d = sh_array_get(a, i);
        if (d->id == id) {
            return d;
        }
    }
    return NULL;
}

static inline void
buff_delay_del(struct buff_delay *d) {
    d->id = 0;
}

static struct buff_delay *
buff_delay_add(struct sh_array *a, uint32_t id) {
    struct buff_delay *d = buff_delay_find(a, 0);
    if (d) {
        d->id = id;
    } else {
        d = sh_array_push(a);
    }
    d->id = id;
    return d;
}

// buff effect
static struct buff_effect *
buff_effect_find(struct sh_array *a, uint32_t id) {
    int i;
    for (i=0; i<a->nelem; ++i) {
        struct buff_effect *e = sh_array_get(a, i);
        if (e->id == id)
            return e;
    }
    return NULL;
}

static inline void
buff_effect_del(struct buff_effect *e) {
    e->id = 0;
}

static struct buff_effect *
buff_effect_add(struct sh_array *a, uint32_t id) {
    struct buff_effect *e = buff_effect_find(a, 0);
    if (e) {
        e->id = id;
    } else {
        e = sh_array_push(a);
    }
    e->id = id;
    return e;
}

#endif
