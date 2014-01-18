#ifndef __sh_hash_h__
#define __sh_hash_h__

#include <stdint.h>

// hash32
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

void sh_hash_init(struct sh_hash *h, uint32_t init);
void sh_hash_fini(struct sh_hash *h);
struct sh_hash * sh_hash_new(uint32_t init);
void sh_hash_delete(struct sh_hash *h);
void * sh_hash_find(struct sh_hash *h, uint32_t key);
int sh_hash_insert(struct sh_hash *h, uint32_t key, void *pointer);
void * sh_hash_remove(struct sh_hash *h, uint32_t key);
void sh_hash_foreach(struct sh_hash *h, void (*cb)(void *pointer));
void sh_hash_foreach2(struct sh_hash *h, void (*cb)(void *pointer, void *ud), void *ud);

// hash64
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

void sh_hash64_init(struct sh_hash64 *h, uint64_t init);
void sh_hash64_fini(struct sh_hash64 *h);
void *sh_hash64_find(struct sh_hash64 *h, uint64_t key);
int sh_hash64_insert(struct sh_hash64 *h, uint64_t key, void *pointer);
void * sh_hash64_remove(struct sh_hash64 *h, uint64_t key);
void sh_hash64_foreach(struct sh_hash64 *h, void (*cb)(void *pointer));
void sh_hash64_foreach2(struct sh_hash64 *h, void (*cb)(void *pointer, void *ud), void *ud);
   
#endif
