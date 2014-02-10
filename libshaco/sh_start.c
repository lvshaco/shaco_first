#include "sh.h"
#include "sh_log.h"
#include "sh_timer.h"
#include "sh_net.h"
#include "sh_reload.h"
#include <stdbool.h>

static bool RUN = false;

void
sh_start() {
    sh_info("Shaco start");
    int timeout;
    RUN = true;
    while (RUN) {
        timeout = sh_timer_max_timeout();
        sh_net_poll(timeout);
        sh_timer_dispatch_timeout();
        sh_reload_execute();
    }
    sh_info("Shaco stop");
}

void
sh_stop() {
    RUN = false;
}
