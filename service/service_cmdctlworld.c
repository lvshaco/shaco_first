#include "sc_service.h"
#include "cmdctl.h"
#include "args.h"
#include "player.h"
#include <stdlib.h>

static int
_testrank(struct cmdctl* self, struct args* A, struct memrw* rw) {
    int handler = service_query_id("rank");
    if (handler == SERVICE_INVALID) {
        return CTL_NOSERVICE;
    }
    if (A->argc <= 2) {
        return CTL_ARGLESS;
    }
    uint32_t charid = strtol(A->argv[1], NULL, 10);
    struct player* p = _getplayerbycharid(charid);
    if (p == NULL) {
        return CTL_ARGINVALID;
    }
    struct service_message sm;
    sm.p1 = "dashi";
    sm.p2 = "";
    sm.i1 = charid;
    sm.n1 = strtol(A->argv[1], NULL, 10);
    service_notify_service(handler, &sm);

    sm.p1 = "xinshou";
    sm.p2 = "";
    service_notify_service(handler, &sm);
    return CTL_OK;
}


static struct ctl_command COMMAND_MAP[] = {
    { "testrank", _testrank },
    { NULL, NULL },
};

void
cmdctlworld_service(struct service* s, struct service_message* sm) {
    sm->msg = COMMAND_MAP;
}
