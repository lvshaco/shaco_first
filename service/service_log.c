#include "host_service.h"
#include "host_timer.h"
#include "host_log.h"
#include "host.h"
#include <stdio.h>
#include <time.h>

static inline void
_log_one(int level, const char* log) {
    char buf[64];
    uint64_t now = host_timer_now();
    time_t sec = now / 1000;
    uint32_t msec = now % 1000;
    int off = strftime(buf, sizeof(buf), "%y%m%d-%H:%M:%S.", localtime(&sec));
    snprintf(buf+off, sizeof(buf)-off, "%03d", msec);
    printf("SERV %s %s: %s\n", buf, host_log_levelstr(level), log);
}

int
log_init(struct service* s) {
    const char* level = host_getstr("host_loglevel", "");
    char msg[64];
    snprintf(msg, sizeof(msg), "host log level %s", level);
    _log_one(LOG_INFO, msg);
    return 0;
}

void
log_service(struct service* s, struct service_message* sm) {
    int level = sm->sessionid;
    _log_one(level, sm->msg);     
}
