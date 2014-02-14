#include "sh.h"
#include "sh_log.h"
#include "sh_timer.h"
#include "sh_net.h"
#include "sh_reload.h"
#include <stdbool.h>

static bool RUN = false;
static char STOP_INFO[128];

void
sh_start() {
    sh_info("Shaco start");
    int timeout;
    STOP_INFO[0] = '\0';
    RUN = true; 
    while (RUN) {
        timeout = sh_timer_max_timeout();
        sh_net_poll(timeout);
        sh_timer_dispatch_timeout();
        sh_reload_execute();
    }
    sh_info("Shaco stop(%s)", STOP_INFO);
}

void
sh_stop(const char *info) {
    RUN = false;
    sh_strncpy(STOP_INFO, info, sizeof(STOP_INFO));
}
