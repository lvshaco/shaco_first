#ifndef __host_timer_h__
#define __host_timer_h__

#include <stdint.h>

int host_timer_init();
void host_timer_fini();

int host_timer_max_timeout();
void host_timer_dispatch_timeout();
void host_timer_register(int serviceid, int interval);
uint64_t host_timer_now();
uint64_t host_timer_elapsed();

#endif
