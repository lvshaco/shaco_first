#include "host_service.h"
#include "host_dispatcher.h"
#include "host_log.h"
#include "host_node.h"
#include "node_type.h"
#include "user_message.h"
#include <stdlib.h>
#include <string.h>

struct cmdctl {
    int cmds_service;
};

struct cmdctl*
cmdctl_create() {
    struct cmdctl* self = malloc(sizeof(*self));
    self->cmds_service = -1;
    return self;
}

void
cmdctl_free(struct cmdctl* self) {
    free(self);
}

int
cmdctl_init(struct service* s) {
    struct cmdctl* self = SERVICE_SELF;
    SUBSCRIBE_MSG(s->serviceid, UMID_CMD_REQ);

    if (HNODE_TID(host_id()) == NODE_CENTER) {
        self->cmds_service = service_query_id("cmds");
        if (self->cmds_service == -1) {
            host_error("lost cmds service");
            return 1;
        }
    } else {
        self->cmds_service = -1;
    }
    return 0;
}

static void
_cmdreq(struct cmdctl* self, int id, struct UM_base* um) {
    UM_CAST(UM_cmd_req, req, um);
    if (req->msgsz <= sizeof(*req)) {
        return; // null
    }
    char* cmd = (char*)(req+1);
    cmd[um->msgsz-sizeof(*req)-1] = '\0';

    UM_DEFVAR(UM_cmd_res, res, UMID_CMD_RES);
    res->nodeid = host_id();
    char* str = (char*)(res+1);
    memcpy(str, "ok", 3);
    res->msgsz = sizeof(*res)+3;
    if (self->cmds_service == -1) {
        UM_SEND(id, res, res->msgsz);
    } else {
        service_notify_nodemsg(self->cmds_service, -1, res, res->msgsz);
    }
}

void
cmdctl_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct cmdctl* self = SERVICE_SELF;
    UM_CAST(UM_base, um, msg);
    switch (um->msgid) {
    case UMID_CMD_REQ:
        _cmdreq(self, id, msg);
        break;
    }
}
