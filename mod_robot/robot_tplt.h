#ifndef __robot_tplt_h__
#define __robot_tplt_h__

#include "tplt_include.h"
#include "tplt_struct.h"

struct module;
struct robot;

int robot_tplt_init(struct robot *self);
void robot_tplt_fini(struct robot *self);
void robot_tplt_main(struct module *s, int session, int source, int type, const void *msg, int sz);

#endif
