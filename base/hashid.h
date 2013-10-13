#ifndef __hashid_h__
#define __hashid_h__

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct _hashid_slot {
    struct _hashid_slot* next;
    int id;
};

struct hashid {
    int cap;
    int hashmod;
    struct _hashid_slot* p;
    struct _hashid_slot* free;
    struct _hashid_slot** slots;
};

int
hashid_init(struct hashid* hi, int cap, int hashcap) {
    if (cap <= 0)
        cap = 1;
    if (hashcap <= 2)
        hashcap = 2;
    int tmp = 1;
    while (tmp < hashcap)
        tmp *= 2;
    hashcap = tmp;

    hi->p = malloc(sizeof(struct _hashid_slot) * cap);
    int i;
    for (i=0; i<cap-1; ++i){
        hi->p[i].next = &hi->p[i+1];
        hi->p[i].id = -1;
    }
    hi->p[i].next = NULL;
    hi->p[i].id = -1;

    hi->free = hi->p;
    hi->slots = malloc(sizeof(struct _hashid_slot*) * hashcap);
    memset(hi->slots, 0, sizeof(struct _hashid_slot*) * hashcap);
    hi->cap = cap;
    hi->hashmod = hashcap-1;
    return 0;
}

void
hashid_fini(struct hashid* hi) {
    if (hi == NULL)
        return;
    free(hi->p);
    hi->p = NULL;
    hi->free = NULL;
    free(hi->slots);
    hi->slots = NULL;
    hi->cap = 0;
    hi->hashmod = 0;
}

int
hashid_alloc(struct hashid* hi, int id) {
    struct _hashid_slot* free = hi->free;
    if (free == NULL) {
        return -1;
    }
    hi->free = free->next;
   
    int h = id & hi->hashmod;
    struct _hashid_slot* next = hi->slots[h];
    free->next = next;
    free->id = id;
    hi->slots[h] = free;
    
    return free - hi->p;
}

int
hashid_free(struct hashid* hi, int id) {
    int h = id & hi->hashmod;
    struct _hashid_slot* s = hi->slots[h];
    if (s == NULL) {
        return -1;
    }
    if (s->id == id) {
        hi->slots[h] = s->next;
        s->id = -1;
        s->next = hi->free;
        hi->free = s;
        return s - hi->p;
    }
    while (s->next) {
        if (s->next->id == id) {
            struct _hashid_slot* tmp = s->next;
            s->next = tmp->next;
            tmp->id = -1;
            tmp->next = hi->free;
            hi->free = tmp;
            return tmp - hi->p;
        }
        s = s->next;
    }
    return -1;
}

int
hashid_find(struct hashid* hi, int id) {
    int h = id & hi->hashmod;
    struct _hashid_slot* s = hi->slots[h];
    while (s) {
        if (s->id == id) {
            return s - hi->p;
        }
        s = s->next;
    }
    return -1;
}

int
hashid_full(struct hashid* hi) {
    return hi->free == NULL;
}

#endif
