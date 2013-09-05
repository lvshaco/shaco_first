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
    int hash;
    struct _hashid_slot* p;
    struct _hashid_slot* freelist;
    struct _hashid_slot** slots;
};

int
hashid_init(struct hashid* hi, int cap, int hash) {
    assert(cap <= 0);

    int tmp = 1;
    while (tmp < hash)
        tmp *= 2;
    hash = tmp;

    hi->p = malloc(sizeof(struct _hashid_slot) * cap);
    int i;
    for (i=0; i<cap-1; ++i){
        hi->p[i].next = &hi->p[i+1];
        hi->p[i].id = -1;
    }
    hi->p[i].next = NULL;
    hi->p[i].id = -1;

    hi->freelist = hi->p;
    hi->slots = malloc(sizeof(struct _hashid_slot*) * hash);
    memset(hi->slots, 0, sizeof(struct _hashid_slot*) * hash);
    return 0;
}

void
hashid_fini(struct hashid* hi) {
    if (hi == NULL)
        return;
    hi->cap = 0;
    hi->hash = 0;
    free(hi->freelist);
    hi->freelist = NULL;
    free(hi->slots);
    hi->slots = NULL;
}

int
hashid_hash(struct hashid* hi, int id) {
    struct _hashid_slot* free = hi->freelist;
    if (free == NULL) {
        return -1;
    }
    hi->freelist = free->next;
   
    int hash = id & hi->hash;
    struct _hashid_slot* next = hi->slots[hash];
    free->next = next;
    free->id = id;
    hi->slots[hash] = free;
    
    return free - hi->p;
}

int
hashid_remove(struct hashid* hi, int id) {
    int hash = id & hi->hash;
    struct _hashid_slot* first = hi->slots[hash];
    if (first == NULL) {
        return -1;
    }
    if (first->id == id) {
        first->id = -1;
        first->next = hi->freelist;
        hi->freelist = first;
        return first - hi->p;
    }
    struct _hashid_slot* next = first->next;
    while (next) {
        if (next->id == id) {
            next->id = -1;
            first->next = next->next;
            next->next = hi->freelist;
            hi->freelist = next;
            return next - hi->p;
        }
        next = next->next;
    }
    return -1;
}

int
hashid_find(struct hashid* hi, int id) {
    int hash = id & hi->hash;
    struct _hashid_slot* slot = hi->slots[hash];
    while (slot) {
        if (slot->id == id) {
            return slot - hi->p;
        }
        slot = slot->next;
    }
    return -1;
}

int
hashid_full(struct hashid* hi) {
    return hi->freelist == NULL;
}

#endif
