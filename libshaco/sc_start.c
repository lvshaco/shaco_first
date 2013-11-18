#include "sc.h"
#include "sc_log.h"
#include "sc_timer.h"
#include "sc_net.h"
#include "sc_reload.h"
#include <stdbool.h>

static bool RUN = false;

void
sc_start() {
    sc_info("Shaco start");
    int timeout;
    RUN = true;
    while (RUN) {
        timeout = sc_timer_max_timeout();
        sc_net_poll(timeout);
        sc_timer_dispatch_timeout();
        sc_reload_execute();
    }
    sc_info("Shaco stop");
}

void
sc_stop() {
    RUN = false;
}
