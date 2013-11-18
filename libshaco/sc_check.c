#include "sc_init.h"
#include "sc.h"
#include "sc_env.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

static void
sc_check_init() {
    struct rlimit l;
    
    if (getrlimit(RLIMIT_CORE, &l) == -1) {
        sc_exit("getrlimit core fail: %s", strerror(errno));
    }
    if (l.rlim_cur != RLIM_INFINITY) {
        l.rlim_cur = RLIM_INFINITY;
        l.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &l) == -1) {
            sc_exit("setrlimit fail: %s", strerror(errno));
        }
    }

    int max = sc_getint("sc_connmax", 0) + 1000;
    if (getrlimit(RLIMIT_NOFILE, &l) == -1) {
        sc_exit("getrlimit nofile fail: %s", strerror(errno));
    }
    if (l.rlim_cur < max) {
        l.rlim_cur = max;
        if (l.rlim_max < l.rlim_cur) {
            l.rlim_max = l.rlim_cur;
        }
        if (setrlimit(RLIMIT_NOFILE, &l) == -1) {
            sc_exit("setrlimit nofile fail: %s", strerror(errno));
        }
    }
}

SC_LIBRARY_INIT_PRIO(sc_check_init, NULL, 15);
