#ifndef __MAP_H__
#define __MAP_H__

#include <stdint.h>

struct idmap;
struct idmap* idmap_create(uint32_t cap);
void idmap_free(struct idmap* self);
void* idmap_find(struct idmap* self, uint32_t key);
void idmap_insert(struct idmap* self, uint32_t key, void* pointer);
void* idmap_remove(struct idmap* self, uint32_t key);
void idmap_foreach(struct idmap* self, void (*cb)(uint32_t key, void* value, void* ud), void* ud);

struct strmap;
struct strmap* strmap_create(uint32_t cap);
void strmap_free(struct strmap* self);
void* strmap_find(struct strmap* self, const char* key);
void strmap_insert(struct strmap* self, const char* key, void* pointer);
void* strmap_remove(struct strmap* self, const char* key);
void strmap_foreach(struct strmap* self, void (*cb)(const char* key, void* value, void* ud), void* ud);

#endif
