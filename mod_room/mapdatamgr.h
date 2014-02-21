#ifndef __mapdatamgr_h__
#define __mapdatamgr_h__

#include <stdint.h>

struct sh_hash;
struct roommap;
struct tplt;

struct sh_hash *mapdatamgr_init(struct tplt *T, const char *path);
void mapdatamgr_fini(struct sh_hash *H);
struct roommap *mapdatamgr_find(struct sh_hash *H, uint32_t id);

#endif
