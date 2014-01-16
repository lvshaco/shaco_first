#ifndef __mapdatamgr_h__
#define __mapdatamgr_h__

int  mapdatamgr_init(const char *path);
void mapdatamgr_fini();
struct roommap *mapdatamgr_find(uint32_t id);

#endif
