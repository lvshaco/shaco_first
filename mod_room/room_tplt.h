#ifndef __room_tplt_h__
#define __room_tplt_h__

#include "tplt_include.h"
#include "tplt_struct.h"

struct service;
struct room;

int room_tplt_init(struct room* self);
void room_tplt_fini(struct room* self);
void room_tplt_main(struct service *s, int session, int source, int type, const void *msg, int sz);

#endif
