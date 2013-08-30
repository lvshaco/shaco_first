#include "host_log.h"
/*
#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

void host_log(int level, const char* fmt, ...);


void 
host_log(int level, const char* fmt, ...) {
    char log[1024] = {0};
    int n;

    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(log, sizeof(log), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return; // output error
    }
    if (n >= sizeof(log)) {
        // truncate
    }
    // notify service_log handle
    //printf("n %d.\n", n);
    //printf(log);
}
*/
