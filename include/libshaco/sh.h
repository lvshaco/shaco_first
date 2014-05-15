#ifndef __sh_h__
#define __sh_h__

#include "sh_env.h"
#include "sh_init.h"
#include "sh_log.h"
#include "sh_net.h"
#include "sh_node.h"
#include "sh_reload.h"
#include "sh_module.h"
#include "sh_timer.h"
#include "sh_util.h"
#include "sh_hash.h"
#include "sh_array.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>

void sh_init();
void sh_start();
void sh_stop(const char* info);

void sh_exit(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
__attribute__((noreturn))
#endif
;

void sh_panic(const char* fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
__attribute__((noreturn))
#endif
;

#endif
