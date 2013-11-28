#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint32_t
_str_hash(const char* key) {
    uint32_t len = strlen(key);
    uint32_t h = len;
    uint32_t step = (len>>5)+1;
    uint32_t i;
    for (i=len; i>=step; i-=step)
        h = h ^ ((h<<5)+(h>>2)+(uint32_t)key[i-1]);
    return h;   
}

/*
 * idmap
 */
struct idelement {
    uint32_t key;
    void* pointer;
    struct idelement* next;
};

struct idmap {
    uint32_t used;
    uint32_t cap;
    struct idelement** slots;
};

struct idmap* 
idmap_create(uint32_t cap) {
    uint32_t tmp;
    tmp = 2;
    while (tmp < cap)
        tmp *= 2;
    cap = tmp;

    struct idmap* m = malloc(sizeof(*m));

    struct idelement** slots = malloc(sizeof(struct idelement*) * cap);
    memset(slots, 0, sizeof(struct idelement*) * cap);
    m->slots = slots;
    m->used = 0;
    m->cap = cap;
    return m;
}

void 
idmap_free(struct idmap* self, void (*cb)(void* value)) {
    if (self == NULL)
        return;
   
    struct idelement* e, *tmp;
    uint32_t i;
    for (i=0; i<self->cap; ++i) {
        e = self->slots[i];
        while (e) {
            tmp = e;
            e = e->next;
            if (cb) {
                cb(tmp->pointer);
            }
            free(tmp);
        }
    }
    free(self->slots);
    free(self);
}

void* 
idmap_find(struct idmap* self, uint32_t key) {
    uint32_t hash = key & (self->cap - 1);
    struct idelement* e = self->slots[hash];
    while (e) {
        if (e->key == key) {
            return e->pointer;
        }
        e = e->next;
    }
    return NULL;
}

static void
_idmap_rehash(struct idmap* self) {
    uint32_t oldcap = self->cap;
    self->cap *= 2;
    self->slots = realloc(self->slots, sizeof(struct idelement*) * self->cap);
    memset(self->slots + oldcap, 0, sizeof(struct idelement*) * (self->cap - oldcap));
    struct idelement* e;
    struct idelement* next;
    uint32_t hash;
    int i;
    for (i=0; i<oldcap; ++i) {
        e = self->slots[i];
        self->slots[i] = NULL;
        while (e) {
            next = e->next;
            hash = e->key & (self->cap-1);
            e->next = self->slots[hash];
            self->slots[hash] = e;
            e = next;
        }
    }
}

void 
idmap_insert(struct idmap* self, uint32_t key, void* pointer) {
    if (self->used >= self->cap) {
        _idmap_rehash(self); 
    }
    uint32_t hash = key & (self->cap - 1);
    struct idelement* e = malloc(sizeof(*e));
    e->key = key;
    e->pointer = pointer;
    e->next = self->slots[hash];
    self->slots[hash] = e;
    self->used++;
}

void* 
idmap_remove(struct idmap* self, uint32_t key) {
    void* ret;
    uint32_t hash = key & (self->cap - 1);
    struct idelement** p = &self->slots[hash];
    struct idelement* e = *p;
    while (e) {
        if (e->key == key) {
            *p = e->next;
            ret = e->pointer;
            free(e);
            self->used--;
            return ret;
        }
        p = &e->next;
        e = *p;
    }
    return NULL;
}

void 
idmap_foreach(struct idmap* self, void (*cb)(uint32_t key, void* value, void* ud), void* ud) {
    struct idelement* e, *tmp;
    uint32_t i;
    for (i=0; i<self->cap; ++i) {
        e = self->slots[i];
        while (e) {
            tmp = e;
            e = e->next;
            cb(tmp->key, tmp->pointer, ud);
        }
    }
}

/*
 * strmap
 */
struct strelement {
    const char* key;
    uint32_t hash;
    void* pointer;
    struct strelement* next;
};

struct strmap {
    uint32_t used;
    uint32_t cap;
    struct strelement** slots;
};

struct strmap* 
strmap_create(uint32_t cap) {
    uint32_t tmp;
    tmp = 2;
    while (tmp < cap)
        tmp *= 2;
    cap = tmp;

    struct strmap* m = malloc(sizeof(*m));

    struct strelement** slots = malloc(sizeof(struct idelement*) * cap);
    memset(slots, 0, sizeof(struct idelement*) * cap);
    m->slots = slots;
    m->used = 0;
    m->cap = cap;
    return m;
}

void 
strmap_free(struct strmap* self, void (*cb)(void* value)) {
    if (self == NULL)
        return;

    struct strelement* e, *tmp;
    uint32_t i;
    for (i=0; i<self->cap; ++i) {
        e = self->slots[i];
        while (e) {
            tmp = e;
            e = e->next;
            if (cb) {
                cb(tmp->pointer);
            }
            free(tmp);
        }
    }

    free(self->slots);
    free(self);
}

void* 
strmap_find(struct strmap* self, const char* key) {
    uint32_t hash = _str_hash(key);
    uint32_t hashmod = hash & (self->cap-1);
    struct strelement* e = self->slots[hashmod];
    while (e) {
        if (e->hash == hash && strcmp(e->key, key) == 0) {
            return e->pointer;
        }
        e = e->next;
    }
    return NULL;
}

static void
_strmap_rehash(struct strmap* self) {
    uint32_t oldcap = self->cap;
    self->cap *= 2;
    self->slots = realloc(self->slots, sizeof(struct idelement*) * self->cap);
    memset(self->slots + oldcap, 0, sizeof(struct idelement*) * (self->cap - oldcap));
    struct strelement* e;
    struct strelement* next;
    uint32_t hash;
    int i;
    for (i=0; i<oldcap; ++i) {
        e = self->slots[i];
        self->slots[i] = NULL;
        while (e) {
            next = e->next;
            hash = e->hash & (self->cap-1);
            e->next = self->slots[hash];
            self->slots[hash] = e;
            e = next;
        }
    }
}

void 
strmap_insert(struct strmap* self, const char* key, void* pointer) {
    if (self->used >= self->cap) {
        _strmap_rehash(self); 
    }
    uint32_t hash = _str_hash(key);
    uint32_t hashmod = hash & (self->cap-1);
    struct strelement* e = malloc(sizeof(*e));
    e->key = key;
    e->hash = hash;
    e->pointer = pointer;
    e->next = self->slots[hashmod];
    self->slots[hashmod] = e;
    self->used++;
}

void* 
strmap_remove(struct strmap* self, const char* key) {
    void* ret;
    uint32_t hash = _str_hash(key);
    uint32_t hashmod = hash & (self->cap-1);
    struct strelement** p = &self->slots[hashmod];
    struct strelement* e = *p;
    while (e) {
        if (e->hash == hash && strcmp(e->key, key) == 0) {
            *p = e->next;
            ret = e->pointer;
            free(e);
            return ret;
        }
        p = &e->next;
        e = *p;
    }
    return NULL;
}

void 
strmap_foreach(struct strmap* self, void (*cb)(const char* key, void* value, void* ud), void* ud) {
    struct strelement* e, *tmp;
    uint32_t i;
    for (i=0; i<self->cap; ++i) {
        e = self->slots[i];
        while (e) {
            tmp = e;
            e = e->next;
            cb(tmp->key, tmp->pointer, ud);
        }
    }
}
