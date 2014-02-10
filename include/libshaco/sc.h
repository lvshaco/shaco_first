#ifndef __sc_h__
#define __sc_h__

#include "sc_env.h"
#include "sc_init.h"
#include "sc_log.h"
#include "sc_net.h"
#include "sc_node.h"
#include "sc_reload.h"
#include "sc_service.h"
#include "sc_timer.h"
#include "sh_monitor.h"
#include "sh_util.h"
#include "sh_hash.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>

void sc_init();
void sc_start();
void sc_stop();

void sc_exit(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
__attribute__((noreturn))
#endif
;

void sc_panic(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
__attribute__((noreturn))
#endif
;

#endif
