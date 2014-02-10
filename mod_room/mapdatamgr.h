#ifndef __mapdatamgr_h__
#define __mapdatamgr_h__

#include <stdint.h>

struct roommap;
struct tplt;

int  mapdatamgr_init(struct tplt *T, const char *path);
void mapdatamgr_fini();
struct roommap *mapdatamgr_find(uint32_t id);

#endif
