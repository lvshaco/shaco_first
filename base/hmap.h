#ifndef __HMAP_H__
#define __HMAP_H__

#include <stdint.h>

struct idhmap;
struct idhmap* idhmap_create(uint32_t cap);
void idhmap_free(struct idhmap* self);
void* idhmap_find(struct idhmap* self, uint32_t key);
void idhmap_insert(struct idhmap* self, uint32_t key, void* pointer);
//void* idhmap_remove(struct idhmap* self, uint32_t key);
void idhmap_foreach(struct idhmap* self, void (*cb)(uint32_t key, void* value, void* ud), void* ud);

struct strhmap;
struct strhmap* strhmap_create(uint32_t cap);
void strhmap_free(struct strhmap* self);
void* strhmap_find(struct strhmap* self, const char* key);
void strhmap_insert(struct strhmap* self, const char* key, void* pointer);
//void* strhmap_remove(struct strhmap* self, const char* key);
void strhmap_foreach(struct strhmap* self, void (*cb)(const char* key, void* value, void* ud), void* ud);

#endif
