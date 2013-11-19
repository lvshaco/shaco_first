#ifndef __sc_timer_h__
#define __sc_timer_h__

#include <stdint.h>

int sc_timer_max_timeout();
void sc_timer_dispatch_timeout();
void sc_timer_register(int serviceid, int interval);
uint64_t sc_timer_now();
uint64_t sc_timer_elapsed();
uint64_t sc_timer_elapsed_real();

#endif
