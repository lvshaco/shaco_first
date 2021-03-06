#include "sc_log.h"
#include "sc.h"
#include "sc_env.h"
#include "sc_init.h"
#include "sc_timer.h"
#include "sc_service.h"
#include "sc_init.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <execinfo.h>
#include <assert.h>

static int _LEVEL = LOG_INFO;
static int _LOG_SERVICE = SERVICE_INVALID;

static const char* STR_LEVELS[LOG_MAX] = {
    "DEBUG", "INFO", "WARNING", "ERROR", "REC", "EXIT", "PANIC",
};

static inline const char*
_levelstr(int level) {
    if (level >= 0 && level < LOG_MAX)
        return STR_LEVELS[level];
    return "";
}

int sc_log_level() {
    return _LEVEL;
}

const char* 
sc_log_levelstr(int level) {
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
sc_log_setlevelstr(const char* level) {
    int id = _levelid(level);
    if (id == -1)
        return -1;
    _LEVEL = id;
    return id;
}

static int
_prefix(int level, char* buf, int sz) {
    uint64_t now = sc_timer_now();
    time_t sec = now / 1000;
    uint32_t msec = now % 1000;
    int n;
    n  = snprintf(buf, sz, "[%d ", (int)getpid());
    n += strftime(buf+n, sz-n, "%y%m%d-%H:%M:%S.", localtime(&sec));
    n += snprintf(buf+n, sz-n, "%03d] %s: ", msec, _levelstr(level));
    return n;
}

static void
_log(int level, char* log, int sz) {
    if (_LOG_SERVICE < 0) {
        fprintf(stderr, log);
        return;
    }
    struct service_message sm;
    sm.sessionid = level; // reuse for level
    sm.source = SERVICE_HOST;
    sm.sz = sz;
    sm.msg = log;
    service_notify_service(_LOG_SERVICE, &sm);
}

static void
sc_logv(int level, const char* fmt, va_list ap) {
    char buf[1024] = {0};
    int n;
    n = _prefix(level, buf, sizeof(buf));
    n += vsnprintf(buf+n, sizeof(buf)-n, fmt, ap);
    n += snprintf(buf+n, sizeof(buf)-n, "\n");
    _log(level, buf, n);
}

static void
sc_log(int level, const char* log) {
    char buf[1024] = {0};
    int n;
    n = _prefix(level, buf, sizeof(buf));
    n += snprintf(buf+n, sizeof(buf)-n, "%s\n", log);
    _log(level, buf, n);
}

void 
sc_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_ERROR, fmt, ap);
    va_end(ap);
}

void 
sc_warning(const char* fmt, ...) {
    if (_LEVEL > LOG_WARNING)
        return;
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_WARNING, fmt, ap);
    va_end(ap);
}

void 
sc_info(const char* fmt, ...) {
    if (_LEVEL > LOG_INFO)
        return;
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_INFO, fmt, ap);
    va_end(ap);
}

void 
sc_debug(const char* fmt, ...) {
    if (_LEVEL > LOG_DEBUG)
        return;
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void 
sc_rec(const char* fmt, ...) {
    if (_LEVEL > LOG_REC)
        return;
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_REC, fmt, ap);
    va_end(ap);
}

void 
sc_exit(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_EXIT, fmt, ap);
    va_end(ap);
    exit(1);
}

void
sc_log_backtrace() {
    void* addrs[24];
    int i, n;
    char** symbols;
    n = backtrace(addrs, sizeof(addrs)/sizeof(addrs[0]));
    symbols = backtrace_symbols(addrs, n);
    assert(symbols);
    for (i=0; i<n; ++i) {
        sc_log(LOG_PANIC, symbols[i]);
    }
}

void 
sc_panic(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sc_logv(LOG_PANIC, fmt, ap);
    va_end(ap);

    sc_log(LOG_PANIC, "Panic detected at:");
    sc_log_backtrace();
    abort();
}

static void
sc_log_init() {
    const char* level; 
    _LOG_SERVICE = service_query_id("log");
    if (_LOG_SERVICE != SERVICE_INVALID) {
        if (service_prepare("log")) {
            _LOG_SERVICE = SERVICE_INVALID;
            sc_exit("log init fail");
        }
    } else {
        sc_warning("lost log service");
    }
    level = sc_getstr("sc_loglevel", "");
    sc_log_setlevelstr(level); 
}

static void 
sc_log_fini() {
    _LOG_SERVICE = SERVICE_INVALID;
}

SC_LIBRARY_INIT_PRIO(sc_log_init, sc_log_fini, 12)
