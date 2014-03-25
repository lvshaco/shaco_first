#include "sh_hash.h"
#include <stdlib.h>
#include <string.h>

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

    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            free(one);
            one = next;
        }
    }
    free(h->slots);
    h->slots = NULL;
    h->used = 0;
    h->cap = 0;
}

struct sh_hash *
sh_hash_new(uint32_t init) {
    struct sh_hash *h = malloc(sizeof(*h));
    memset(h, 0, sizeof(*h));
    sh_hash_init(h, init);
    return h;
}

void
sh_hash_delete(struct sh_hash *h) {
    sh_hash_fini(h);
    free(h);
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
    uint32_t old = h->cap;
    h->cap *= 2;
    h->slots = realloc(h->slots, sizeof(h->slots[0]) * h->cap);
    memset(h->slots + old, 0, sizeof(h->slots[0]) * (h->cap - old));
   
    uint32_t hash; 
    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<old; ++i) {
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
    struct sh_hash_slot *tmp;
    while (*p) {
        if ((*p)->key == key) {
            pointer = (*p)->pointer;
            tmp = *p;
            *p = (*p)->next;
            free(tmp);
            h->used--;
            return pointer;
        }
        p = &(*p)->next;
    }
    return NULL;
}

void *
sh_hash_pop(struct sh_hash *h) {
    struct sh_hash_slot *one;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        if (one) {
            return sh_hash_remove(h, one->key);
        }
    }
    return NULL;
}

void
sh_hash_foreach(struct sh_hash *h, void (*cb)(void *pointer)) {
    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            (cb)(one->pointer);
            one = next;
        }
    }
}

void
sh_hash_foreach2(struct sh_hash *h, void (*cb)(void *pointer, void *ud), void *ud) {
    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            (cb)(one->pointer, ud);
            one = next;
        }
    }
}

void
sh_hash_foreach3(struct sh_hash *h, void (*cb)(uint32_t key, void *pointer, void *ud), void *ud) {
    struct sh_hash_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            (cb)(one->key, one->pointer, ud);
            one = next;
        }
    }
}

// hash64
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
    uint32_t old = h->cap;
    h->cap *= 2;
    h->slots = realloc(h->slots, sizeof(h->slots[0]) * h->cap);
    memset(h->slots + old, 0, sizeof(h->slots[0]) * (h->cap - old));
   
    uint64_t hash; 
    struct sh_hash64_slot *one, *next;
    int i;
    for (i=0; i<old; ++i) {
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
    struct sh_hash64_slot *tmp;
    while (*p) {
        if ((*p)->key == key) {
            pointer = (*p)->pointer;
            tmp = *p;
            *p = (*p)->next;
            free(tmp);
            h->used--;
            return pointer;
        }
        p = &(*p)->next;
    }
    return NULL;
}

void
sh_hash64_foreach(struct sh_hash64 *h, void (*cb)(void *pointer)) {
    struct sh_hash64_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            (cb)(one->pointer);
            one = next;
        }
    }
}

void
sh_hash64_foreach2(struct sh_hash64 *h, void (*cb)(void *pointer, void *ud), void *ud) {
    struct sh_hash64_slot *one, *next;
    int i;
    for (i=0; i<h->cap; ++i) {
        one = h->slots[i];
        while (one) {
            next = one->next;
            (cb)(one->pointer, ud);
            one = next;
        }
    }
}
