#ifndef __sh_hash_h__
#define __sh_hash_h__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//------------------------------hash32-------------------------------
struct sh_hash_slot {
    uint32_t key;
    void *pointer;
    struct sh_hash_slot* next;
};

struct sh_hash {
    uint32_t used;
    uint32_t cap;
    struct sh_hash_slot** slots;
};

void
sh_hash_init(struct sh_hash *h, uint32_t init) {
    uint32_t cap = 1;
    while (cap < init)
        cap *= 2;
    h->cap = cap;
    h->used = 0;
    h->slots = malloc(sizeof(h->slots[0]) * cap);
    memset(h->slots, 0, sizeof(h->slots[0]) * cap);
}

void
sh_hash_fini(struct sh_hash *h) {
    if (h == NULL)
        return;
    free(h->slots);
    h->slots = NULL;
    h->used = 0;
    h->cap = 0;
}

void *
sh_hash_find(struct sh_hash *h, uint32_t key) {
    uint32_t hash = key & (h->cap-1);
    struct sh_hash_slot *slot = h->slots[hash];
    while (slot) {
        if (slot->key == key) {
            return slot->pointer;
        }
        slot = slot->next;
    }
    return NULL;
}

static void
_rehash(struct sh_hash *h) {
    h->cap *= 2;
    h->slots = realloc(h->slots, sizeof(h->slots[0]) * h->cap);
    memset(h->slots + h->used, 0, sizeof(h->slots[0]) * (h->cap - h->used));
   
    uint32_t hash; 
    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<h->used; ++i) {
        one = h->slots[i];
        h->slots[i] = NULL;
        while (one) {
            next = one->next;
            hash = one->key & (h->cap-1);
            one->next = h->slots[hash];
            h->slots[hash] = one;
            one = next; 
        }
    }
}

int
sh_hash_insert(struct sh_hash *h, uint32_t key, void *pointer) {
    uint32_t hash = key & (h->cap-1);

    struct sh_hash_slot *slot = h->slots[hash];
    while (slot) {
        if (slot->key == key) {
            return 1;
        }
        slot = slot->next;
    }
    if (h->used >= h->cap) {
        _rehash(h);
        hash = key & (h->cap-1);
    }
    struct sh_hash_slot *one = malloc(sizeof(*one)); 
    one->key = key;
    one->pointer = pointer;
    one->next = h->slots[hash];
    h->slots[hash] = one;

    h->used++;
    return 0;
}

void *
sh_hash_remove(struct sh_hash *h, uint32_t key) {
    void *pointer;
    uint32_t hash = key & (h->cap-1);
    struct sh_hash_slot **p = &h->slots[hash];
    while (*p) {
        if ((*p)->key == key) {
            pointer = (*p)->pointer;
            free(*p);
            *p = (*p)->next;

            h->used--;
            return pointer;
        }
        p = &(*p)->next;
    }
    return NULL;
}

//--------------------------------hash64---------------------------------
struct sh_hash64_slot {
    uint64_t key;
    void *pointer;
    struct sh_hash64_slot* next;
};

struct sh_hash64 {
    uint64_t used;
    uint64_t cap;
    struct sh_hash64_slot** slots;
};

void
sh_hash64_init(struct sh_hash64 *h, uint64_t init) {
    uint64_t cap = 1;
    while (cap < init)
        cap *= 2;
    h->cap = cap;
    h->used = 0;
    h->slots = malloc(sizeof(h->slots[0]) * cap);
    memset(h->slots, 0, sizeof(h->slots[0]) * cap);
}

void
sh_hash64_fini(struct sh_hash64 *h) {
    if (h == NULL)
        return;
    free(h->slots);
    h->slots = NULL;
    h->used = 0;
    h->cap = 0;
}

void *
sh_hash64_find(struct sh_hash64 *h, uint64_t key) {
    uint64_t hash = key & (h->cap-1);
    struct sh_hash64_slot *slot = h->slots[hash];
    while (slot) {
        if (slot->key == key) {
            return slot->pointer;
        }
        slot = slot->next;
    }
    return NULL;
}

static void
_rehash64(struct sh_hash64 *h) {
    h->cap *= 2;
    h->slots = realloc(h->slots, sizeof(h->slots[0]) * h->cap);
    memset(h->slots + h->used, 0, sizeof(h->slots[0]) * (h->cap - h->used));
   
    uint64_t hash; 
    struct sh_hash64_slot *one, *next;
    int i;
    for (i=0; i<h->used; ++i) {
        one = h->slots[i];
        h->slots[i] = NULL;
        while (one) {
            next = one->next;
            hash = one->key & (h->cap-1);
            one->next = h->slots[hash];
            h->slots[hash] = one;
            one = next; 
        }
    }
}

int
sh_hash64_insert(struct sh_hash64 *h, uint64_t key, void *pointer) {
    uint64_t hash = key & (h->cap-1);

    struct sh_hash64_slot *slot = h->slots[hash];
    while (slot) {
        if (slot->key == key) {
            return 1;
        }
        slot = slot->next;
    }
    if (h->used >= h->cap) {
        _rehash64(h);
        hash = key & (h->cap-1);
    }
    struct sh_hash64_slot *one = malloc(sizeof(*one)); 
    one->key = key;
    one->pointer = pointer;
    one->next = h->slots[hash];
    h->slots[hash] = one;

    h->used++;
    return 0;
}

void *
sh_hash64_remove(struct sh_hash64 *h, uint64_t key) {
    void *pointer;
    uint64_t hash = key & (h->cap-1);
    struct sh_hash64_slot **p = &h->slots[hash];
    while (*p) {
        if ((*p)->key == key) {
            pointer = (*p)->pointer;
            free(*p);
            *p = (*p)->next;

            h->used--;
            return pointer;
        }
        p = &(*p)->next;
    }
    return NULL;
}


#endif
