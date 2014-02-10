#ifndef __hall_tplt_h__
#define __hall_tplt_h__

#include "tplt_include.h"
#include "tplt_struct.h"

struct module;
struct hall;

int hall_tplt_init(struct hall *self);
void hall_tplt_fini(struct hall *self);
void hall_tplt_main(struct module *s, int session, int source, int type, const void *msg, int sz);

#endif
