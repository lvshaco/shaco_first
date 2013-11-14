#include "host_log.h"
#include "host_timer.h"
#include "host_service.h"
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static int _LEVEL = LOG_INFO;
static int _LOG_SERVICE = SERVICE_INVALID;

static const char* STR_LEVELS[LOG_MAX] = {
    "DEBUG", "INFO", "WARNING", "ERROR",
};

static inline const char*
_levelstr(int level) {
    if (level >= 0 && level < LOG_MAX)
        return STR_LEVELS[level];
    return "";
}

int host_log_level() {
    return _LEVEL;
}

const char* 
host_log_levelstr(int level) {
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
host_log_setlevelstr(const char* level) {
    int id = _levelid(level);
    if (id == -1)
        return -1;
    _LEVEL = id;
    return id;
}

int 
host_log_init(const char* level) {
    _LOG_SERVICE = service_query_id("log");
    if (_LOG_SERVICE != SERVICE_INVALID) {
        if (service_prepare("log"))
            return 1;
    } else {
        host_warning("lost log service");
    }
    host_log_setlevelstr(level);
    return 0;
}

void 
host_log_fini() {
    _LOG_SERVICE = SERVICE_INVALID;
}

#define _gen_message(level, log, n, fmt) \
    char log[2048] = {0}; \
    uint64_t now = host_timer_now(); \
    time_t sec = now / 1000; \
    uint32_t msec = now % 1000; \
    int n, n2; \
    n  = snprintf(log, sizeof(log), "[%d ", (int)getpid()); \
    n += strftime(log+n, sizeof(log)-n, "%y%m%d-%H:%M:%S.", localtime(&sec)); \
    n += snprintf(log+n, sizeof(log)-n, "%03d] %s: ", msec, _levelstr(level)); \
    va_list ap; \
    va_start(ap, fmt); \
    n += vsnprintf(log+n, sizeof(log)-n, fmt, ap); \
    va_end(ap); \
    n2 = snprintf(log+n, sizeof(log)-n, "\n"); \
    if (n2 < 0) { \
        return; \
    } \
    n += n2;

static void
_send_to_service(int level, char* log, int sz) {
    if (log[0] == '\0') {
        return;
    }
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

void 
host_error(const char* fmt, ...) {
    _gen_message(LOG_ERROR, log, n, fmt);
    _send_to_service(LOG_ERROR, log, n);
}

void 
host_warning(const char* fmt, ...) {
    if (_LEVEL > LOG_WARNING)
        return;
    _gen_message(LOG_WARNING, log, n, fmt);
    _send_to_service(LOG_WARNING, log, n);
}

void 
host_info(const char* fmt, ...) {
    if (_LEVEL > LOG_INFO)
        return;
    _gen_message(LOG_INFO, log, n, fmt);
    _send_to_service(LOG_INFO, log, n);
}

void 
host_debug(const char* fmt, ...) {
    if (_LEVEL > LOG_DEBUG)
        return;
    _gen_message(LOG_DEBUG, log, n, fmt);
    _send_to_service(LOG_DEBUG, log, n);
}
