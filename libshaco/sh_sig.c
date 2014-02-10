#include "sh_init.h"
#include "sh.h"
#include "sh_log.h"
#include <signal.h>
#include <stdlib.h>

static void 
_sigtermhandler(int sig) {
    sh_warning("Received SIGTERM, shhedule stop ...");
    sh_stop();
} 

static void
sh_sig_init() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = _sigtermhandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

}

SH_LIBRARY_INIT_PRIO(sh_sig_init, NULL, 15);
