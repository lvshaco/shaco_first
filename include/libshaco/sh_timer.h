#ifndef __sh_timer_h__
#define __sh_timer_h__

#include <stdint.h>

int sh_timer_max_timeout();
void sh_timer_dispatch_timeout();
void sh_timer_register(int serviceid, int interval);
uint64_t sh_timer_now();
uint64_t sh_timer_elapsed();
uint64_t sh_timer_elapsed_real();

#endif
