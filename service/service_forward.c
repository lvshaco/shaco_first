#include "host_service.h"
#include "host_gate.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include <stdlib.h>
#include <string.h>

static inline void
_forward_world(struct gate_client* c, struct UM_base* um) {
    UM_DEFVAR(UM_forward, fw, UMID_FORWARD);
    fw->cid = c->connid;
    memcpy(&fw->wrap, um, um->msgsz);
    const struct host_node* node = host_node_get(HNODE_ID(NODE_WORLD, 0));
    if (node) {
        UM_SEND(node->connid, fw, UM_forward_size(fw));
    }
}
void
forward_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate_message* gm = msg;
    struct gate_client* c = gm->c;
    UM_CAST(UM_base, um, gm->msg);
    _forward_world(c, um);
}
void
forward_nodemsg(struct service* s, int id, void* msg, int sz) {
    UM_CAST(UM_base, um, msg);
    const struct host_node* node = host_node_get(um->nodeid);
    if (node == NULL)
        return;
    switch (um->msgid) {
    case UMID_FORWARD: {
        UM_CAST(UM_forward, fw, um);
        struct UM_base* m = &fw->wrap;
        struct gate_client* c = host_gate_getclient(fw->cid);
        if (c) {
            if (m->msgid == UMID_LOGOUT) {
                host_gate_disconnclient(c, true);
            } else {
                UM_SENDTOCLI(c->connid, m, m->msgsz);
            }
        }
        break;
        }
    }
}
void
forward_net(struct service* s, struct gate_message* gm) {
    struct net_message* nm = gm->msg;
    UM_DEFFIX(UM_logout, logout, UMID_LOGOUT);
    switch (nm->type) {
    case NETE_SOCKERR:
        logout.type = LOGOUT_SOCKERR;
        _forward_world(gm->c, (struct UM_base*)&logout);
    case NETE_TIMEOUT:
        logout.type = LOGOUT_TIMEOUT;
        _forward_world(gm->c, (struct UM_base*)&logout);
        break;
    }
}
