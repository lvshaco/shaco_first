#include "sc_service.h"
#include "cmdctl.h"
#include <stdlib.h>

static struct ctl_command COMMAND_MAP[] = {
    { NULL, NULL },
};

void
cmdctlgame_service(struct service* s, struct service_message* sm) {
    sm->msg = COMMAND_MAP;
}
