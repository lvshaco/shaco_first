#include "host_log.h"
#include "host_service.h"
#include <stdio.h>
#include <stdarg.h>

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

static inline void
_send_to_service(int level, char* log, int sz) {
    static int logger_service = -1;
    if (logger_service < 0) {
        logger_service = service_query_id("log");
        if (logger_service < 0) {
            return;
        }
    }
    if (log[0] == '\0') {
        return;
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
    _gen_message(log, n, fmt);
    _send_to_service(LOG_WARNING, log, n);
}

void 
host_info(const char* fmt, ...) {
    _gen_message(log, n, fmt);
    _send_to_service(LOG_INFO, log, n);
}

void 
host_debug(const char* fmt, ...) {
    _gen_message(log, n, fmt);
    _send_to_service(LOG_DEBUG, log, n);
}
