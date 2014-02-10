#include "sh_log.h"
#include "sh_util.h"
#include "sc.h"
#include "sh_node.h"
#include "sh_env.h"
#include "sh_init.h"
#include "sh_timer.h"
#include "sh_module.h"
#include "sh_init.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <execinfo.h>
#include <assert.h>

static int _LEVEL = LOG_INFO;
static int _LOG_SERVICE = MODULE_INVALID;

static const char* STR_LEVELS[LOG_MAX] = {
    "DEBUG", "TRACE", "INFO", "WARNING", "ERROR", "REC", "EXIT", "PANIC",
};

static inline const char*
_levelstr(int level) {
    if (level >= 0 && level < LOG_MAX)
        return STR_LEVELS[level];
    return "";
}

int sh_log_level() {
    return _LEVEL;
}

const char* 
sh_log_levelstr(int level) {
    return _levelstr(level);
}

static int 
_levelid(const char* level) {
    int i;
    for (i=LOG_DEBUG; i<LOG_MAX; ++i) {
        if (strcasecmp(STR_LEVELS[i], level) == 0)
            return i;
    }
    return -1;
}

int
sh_log_setlevelstr(const char* level) {
    int id = _levelid(level);
    if (id == -1)
        return -1;
    _LEVEL = id;
    return id;
}

static int
_prefix(int level, char* buf, int sz) {
    uint64_t now = sh_timer_now();
    time_t sec = now / 1000;
    uint32_t msec = now % 1000;
    int n;
    n  = sh_snprintf(buf, sz, "[%d ", (int)getpid());
    n += strftime(buf+n, sz-n, "%y%m%d-%H:%M:%S.", localtime(&sec));
    n += sh_snprintf(buf+n, sz-n, "%03d] %s: ", msec, _levelstr(level));
    return n;
}

static void
_log(int level, char* log, int sz) {
    if (_LOG_SERVICE < 0) {
        fprintf(stderr, log);
        return;
    }
    module_main(_LOG_SERVICE, level, 0, MT_LOG, log, sz);
}

static void
sh_logv(int level, const char* fmt, va_list ap) {
    char buf[1024] = {0};
    int n;
    n = _prefix(level, buf, sizeof(buf));
    n += sh_vsnprintf(buf+n, sizeof(buf)-n, fmt, ap);
    n += sh_snprintf(buf+n, sizeof(buf)-n, "\n");
    _log(level, buf, n);
}

static void
sh_log(int level, const char* log) {
    char buf[1024] = {0};
    int n;
    n = _prefix(level, buf, sizeof(buf));
    n += sh_snprintf(buf+n, sizeof(buf)-n, "%s\n", log);
    _log(level, buf, n);
}

void 
sh_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_ERROR, fmt, ap);
    va_end(ap);
}

void 
sh_warning(const char* fmt, ...) {
    if (_LEVEL > LOG_WARNING)
        return;
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_WARNING, fmt, ap);
    va_end(ap);
}

void 
sh_info(const char* fmt, ...) {
    if (_LEVEL > LOG_INFO)
        return;
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_INFO, fmt, ap);
    va_end(ap);
}

void 
sh_trace(const char* fmt, ...) {
    if (_LEVEL > LOG_TRACE)
        return;
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_TRACE, fmt, ap);
    va_end(ap);
}

void 
sh_debug(const char* fmt, ...) {
    if (_LEVEL > LOG_DEBUG)
        return;
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void 
sh_rec(const char* fmt, ...) {
    if (_LEVEL > LOG_REC)
        return;
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_REC, fmt, ap);
    va_end(ap);
}

void 
sh_exit(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_EXIT, fmt, ap);
    va_end(ap);
    exit(1);
}

void
sh_log_backtrace() {
    void* addrs[24];
    int i, n;
    char** symbols;
    n = backtrace(addrs, sizeof(addrs)/sizeof(addrs[0]));
    symbols = backtrace_symbols(addrs, n);
    assert(symbols);
    for (i=0; i<n; ++i) {
        sh_log(LOG_PANIC, symbols[i]);
    }
}

void 
sh_panic(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sh_logv(LOG_PANIC, fmt, ap);
    va_end(ap);

    sh_log(LOG_PANIC, "Panic detected at:");
    sh_log_backtrace();
    abort();
}

static void
sh_log_init() {
    const char* level; 
    _LOG_SERVICE = module_query_id("log");
    if (_LOG_SERVICE != MODULE_INVALID) {
        if (module_prepare("log")) {
            _LOG_SERVICE = MODULE_INVALID;
            sh_exit("log init fail");
        }
    } else {
        sh_warning("lost log module");
    }
    level = sh_getstr("sh_loglevel", "");
    sh_log_setlevelstr(level); 
}

static void 
sh_log_fini() {
    _LOG_SERVICE = MODULE_INVALID;
}

SH_LIBRARY_INIT_PRIO(sh_log_init, sh_log_fini, 12)
