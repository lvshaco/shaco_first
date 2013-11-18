#include "sc_init.h"
#include "sc.h"
#include "sc_log.h"
#include <signal.h>
#include <stdlib.h>

static void 
_sigtermhandler(int sig) {
    sc_warning("Received SIGTERM, schedule stop ...");
    sc_stop();
} 

static void
sc_sig_init() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = _sigtermhandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

}

SC_LIBRARY_INIT_PRIO(sc_sig_init, NULL, 15);
