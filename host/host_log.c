#include "host_log.h"
#include "host_timer.h"
#include "host_service.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static int _LEVEL = LOG_ERROR;

static const char* STR_LEVELS[LOG_MAX] = {
    "DEBUG", "INFO", "WARNING", "ERROR",
};

static inline const char*
_strlevel(int level) {
    if (level >= 0 && level < LOG_MAX)
        return STR_LEVELS[level];
    return "";
}

const char* 
host_log_levelstr(int level) {
    return _strlevel(level);
}

int 
host_log_levelid(const char* level) {
    int i;
    for (i=LOG_DEBUG; i<LOG_MAX; ++i) {
        if (strcmp(STR_LEVELS[i], level) == 0)
            return i;
    }
    return LOG_DEBUG;
}

void
host_log_setlevel(int level) {
    _LEVEL = level;
}

void 
host_log_setlevelstr(const char* level) {
    _LEVEL = host_log_levelid(level);
}

static inline void
_default_log(int level, const char* log) {
    char buf[64];
    uint64_t now = host_timer_now();
    time_t sec = now / 1000;
    uint32_t msec = now % 1000;
    int off = strftime(buf, sizeof(buf), "%y%m%d-%H:%M:%S.", localtime(&sec));
    snprintf(buf+off, sizeof(buf)-off, "%03d", msec);
    printf("%s %s: %s\n", buf, _strlevel(level), log);
}

#define _gen_message(log, n, fmt) \
    char log[1024] = {0}; \
    int n; \
    do { \
        va_list ap; \
        va_start(ap, fmt); \
        n = vsnprintf(log, sizeof(log), fmt, ap); \
        va_end(ap); \
        if (n < 0) { \
            return; \
        } \
    } while(0)

static void
_send_to_service(int level, char* log, int sz) {
    static int logger_service = -1;
    if (log[0] == '\0') {
        return;
    }
    if (logger_service < 0) {
        logger_service = service_query_id("log");
        if (logger_service < 0) {
            _default_log(level, log);
            return;
        }
    }
    struct service_message sm;
    sm.sessionid = level; // reuse for level
    sm.source = SERVICE_HOST;
    sm.sz = sz;
    sm.msg = log;
    service_notify_service_message(logger_service, &sm);
}

void 
host_error(const char* fmt, ...) {
    _gen_message(log, n, fmt);
    _send_to_service(LOG_ERROR, log, n);
}

void 
host_warning(const char* fmt, ...) {
    if (_LEVEL > LOG_WARNING)
        return;
    _gen_message(log, n, fmt);
    _send_to_service(LOG_WARNING, log, n);
}

void 
host_info(const char* fmt, ...) {
    if (_LEVEL > LOG_INFO)
        return;
    _gen_message(log, n, fmt);
    _send_to_service(LOG_INFO, log, n);
}

void 
host_debug(const char* fmt, ...) {
    if (_LEVEL > LOG_DEBUG)
        return;
    _gen_message(log, n, fmt);
    _send_to_service(LOG_DEBUG, log, n);
}
