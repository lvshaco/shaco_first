#ifndef __roommap_h__
#define __roommap_h__

#include <stdint.h>
#include <stdlib.h>

#pragma pack(1)

struct roommap_header {
    uint16_t width;
    uint16_t height;
};

struct roommap_typeid_header {
    uint8_t offset;
    uint8_t num;
};

struct roommap_typeid {
    uint8_t id;
};

struct roommap_typeidlist {
    struct roommap_typeid* first;
    uint8_t num;
};

struct roommap_cell {
    uint16_t isassign:1; // or is rand typed need rand
    uint16_t dummy:15;
    uint8_t  cellrate;
    uint8_t  itemrate;
    uint32_t cellid;
    uint32_t itemid;
};

struct roommap {
    struct roommap_typeid* typeid_entry;
    struct roommap_cell*   cell_entry;
    struct roommap_header  header;
    char data[];
};

#pragma pack()

#define ROOMMAP_DEPTH(m)        (((m)->header.height+99)/100)
#define ROOMMAP_TID_HEADER(m)   ((struct roommap_typeid_header*)((m)->data))
#define ROOMMAP_TID_ENTRY(m)    ((m)->typeid_entry)
#define ROOMMAP_CELL_ENTRY(m)   ((m)->cell_entry)
#define ROOMMAP_NCELL(m)        ((m)->header.height*(m)->header.width)

static inline struct roommap_typeidlist 
roommap_gettypeidlist(struct roommap* self, uint16_t index) {
    struct roommap_typeidlist tilist;
    if (index < ROOMMAP_DEPTH(self)) {
        struct roommap_typeid_header* th = ROOMMAP_TID_HEADER(self);
        struct roommap_typeid* ti = ROOMMAP_TID_ENTRY(self);
        tilist.first = &ti[th[index].offset];
        tilist.num = th[index].num;
    } else {
        tilist.first = NULL;
        tilist.num = 0;
    }
    return tilist;
}

struct roommap* roommap_create(const char* file);
struct roommap* roommap_createfromstream(void* stream, int sz);
void roommap_free(struct roommap* self);

#endif
