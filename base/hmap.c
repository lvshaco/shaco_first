#include "hmap.h"
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
 * idhmap
 */
struct idhelement {
    uint32_t key;
    void* pointer;
    uint32_t next;
};

struct idhmap {
    uint32_t used;
    uint32_t cap;
    struct idhelement* elems;
};

struct idhmap*
idhmap_create(uint32_t cap) {
    uint32_t tmp = 1;
    while (tmp < cap)
        tmp *= 2;
    cap = tmp;

    struct idhmap* m = malloc(sizeof(struct idhmap));
    m->elems = malloc(sizeof(struct idhelement) * cap);
    memset(m->elems, 0, sizeof(struct idhelement) * cap);
    m->cap = cap;
    m->used = 0;
    return m;
}

void
idhmap_free(struct idhmap* self) {
    if (self == NULL)
        return;
    free(self->elems);
    free(self);
}

void*
idhmap_find(struct idhmap* self, uint32_t key) {
    uint32_t hashi = key & (self->cap-1);
    struct idhelement* e = &self->elems[hashi];
    while (e->pointer) {
        if (e->key == key)
            return e->pointer;
        if (e->next == 0)
            return NULL;
        assert(e->next > 0 && e->next <= self->cap);
        e = &self->elems[e->next-1];
    }
    return NULL;
}

// has enough cap
static void
_idhmap_insert(struct idhmap* self, uint32_t key, void* pointer) {
    uint32_t hashi = key & (self->cap - 1);
    struct idhelement* e = &self->elems[hashi];
    if (e->pointer == NULL) {
        e->key = key;
        e->pointer = pointer;
    } else {
        struct idhelement* next = NULL;
        uint32_t nexti = hashi + 1;
        int i;
        for (i=0; i<self->cap; ++i) {
            nexti = (nexti+i) & (self->cap-1);
            next = &self->elems[nexti];
            if (next->pointer == NULL)
                break;
        }
        assert(next && next->pointer == NULL);
        next->key = key;
        next->pointer = pointer;
        next->next = e->next;
        e->next = nexti + 1;
    } 
}

static void
_idhmap_rehash(struct idhmap* self) {
    struct idhelement* oldelems = self->elems;
    uint32_t oldcap = self->cap;
   
    self->cap *= 2;
    self->elems = malloc(sizeof(struct idhelement) * self->cap);
    memset(self->elems, 0, sizeof(struct idhelement) * self->cap); 

    uint32_t i;
    for (i=0; i<oldcap; ++i) {
        struct idhelement* e = &oldelems[i];
        _idhmap_insert(self, e->key, e->pointer);
    }
    free(oldelems);
}

void
idhmap_insert(struct idhmap* self, uint32_t key, void* pointer) {
    assert(pointer);
    if (self->used >= self->cap) {
        _idhmap_rehash(self);
    }
    _idhmap_insert(self, key, pointer);
    self->used++;
}
/*
void* 
idhmap_remove(struct idhmap* self, uint32_t key) {
    uint32_t hashi = key & (self->cap - 1);
    struct idhelement* e = &self->elems[hashi];
    struct idhelement* prev = NULL;
    while (e->key) {
        if (e->hash == hash && strcmp(e->key, key) == 0) {
            if (prev) {
                prev->next = e->next;
                e->next = 0;
            }
            e->key = NULL;
            self->used--;
            return e->pointer;
        }
        if (e->next <= 0)
            return NULL;
        assert(e->next > 0 && e->next <= self->cap);
        prev = e;
        e = &self->elems[e->next-1];
    }
    return NULL;
}
*/
void 
idhmap_foreach(struct idhmap* self, void (*cb)(uint32_t key, void* value, void* ud), void* ud) {
    int i;
    for (i=0; i<self->cap; ++i) {
        if (self->elems[i].pointer) {
            cb(self->elems[i].key, self->elems[i].pointer, ud);
        }
    }
}

/*
 * strhmap
 */
struct strhelement {
    const char* key;
    uint32_t hash;
    void* pointer;
    uint32_t next;
};

struct strhmap {
    uint32_t used;
    uint32_t cap;
    struct strhelement* elems;
};

struct strhmap*
strhmap_create(uint32_t cap) {
    uint32_t tmp = 1;
    while (tmp < cap) {
        tmp *= 2;
    }
    cap = tmp;

    struct strhmap* m = malloc(sizeof(struct strhmap));
    m->elems = malloc(sizeof(struct strhelement) * cap);
    memset(m->elems, 0, sizeof(struct strhelement) * cap);
    m->cap = cap;
    m->used = 0;
    return m;
}

void
strhmap_free(struct strhmap* self) {
    if (self == NULL)
        return;
    free(self->elems);
    free(self);
}

void*
strhmap_find(struct strhmap* self, const char* key) {
    uint32_t hash = _str_hash(key);
    uint32_t hashi = hash & (self->cap - 1);
    struct strhelement* e = &self->elems[hashi];
    while (e->key) {
        if (e->hash == hash && strcmp(e->key, key) == 0)
            return e->pointer;
        if (e->next == 0)
            return NULL;
        assert(e->next > 0 && e->next <= self->cap);
        e = &self->elems[e->next-1];
    }
    return NULL;
}

// has enough cap
static void
_strhmap_insert(struct strhmap* self, const char* key, uint32_t hash, void* pointer) {
    uint32_t hashi = hash & (self->cap - 1);
    struct strhelement* e = &self->elems[hashi];
    if (e->key == NULL) {
        e->key = key;
        e->hash = hash;
        e->pointer = pointer;
    } else {
        struct strhelement* next = NULL;
        uint32_t nexti = hashi + 1;
        int i;
        for (i=0; i<self->cap; ++i) {
            nexti = (nexti+i) & (self->cap-1);
            next = &self->elems[nexti];
            if (next->key == NULL)
                break;
        }
        assert(next && next->key == NULL);
        next->key = key;
        next->hash = hash;
        next->pointer = pointer;
        next->next = e->next;
        e->next = nexti + 1;
    } 
}

void
_strhmap_rehash(struct strhmap* self) {
    struct strhelement* oldelems = self->elems;
    uint32_t oldcap = self->cap;
   
    self->cap *= 2;
    self->elems = malloc(sizeof(struct strhelement) * self->cap);
    memset(self->elems, 0, sizeof(struct strhelement) * self->cap); 

    uint32_t i;
    for (i=0; i<oldcap; ++i) {
        struct strhelement* e = &oldelems[i];
        _strhmap_insert(self, e->key, e->hash, e->pointer);
    }
    free(oldelems);
}

void
strhmap_insert(struct strhmap* self, const char* key, void* pointer) {
    if (self->used >= self->cap) {
        _strhmap_rehash(self);
    }
    uint32_t hash = _str_hash(key);
    _strhmap_insert(self, key, hash, pointer);
    self->used++;
}
/*
void* 
strhmap_remove(struct strhmap* self, const char* key) {
    uint32_t hash = _str_hash(key);
    uint32_t hashi = hash & (self->cap - 1);
    struct strhelement* e = &self->elems[hashi];
    struct strhelement* prev = NULL;
    while (e->key) {
        if (e->hash == hash && strcmp(e->key, key) == 0) {
            if (prev) {
                prev->next = e->next;
                e->next = 0;
            }
            e->key = NULL;
            self->used--;
            return e->pointer;
        }
        if (e->next <= 0)
            return NULL;
        assert(e->next > 0 && e->next <= self->cap);
        prev = e;
        e = &self->elems[e->next-1];
    }
    return NULL;
}
*/
void 
strhmap_foreach(struct strhmap* self, void (*cb)(const char* key, void* value, void* ud), void* ud) {
    int i;
    for (i=0; i<self->cap; ++i) {
        if (self->elems[i].key) {
            cb(self->elems[i].key, self->elems[i].pointer, ud);
        }
    }
}
