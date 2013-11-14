#ifndef __host_h__
#define __host_h__

#include "host_env.h"
#include <stdbool.h>

int host_create();
void host_free();
void host_start();
void host_stop();

#endif
