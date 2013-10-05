#include "host_service.h"
#include "host_gate.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include <stdlib.h>
#include <string.h>

static inline void
_forward_world(struct gate_client* c, void* msg, int sz) {
    UM_DEFVAR(UM_forward, fw, UMID_FORWARD);
    fw->cid = c->connid;
    memcpy(&fw->wrap, msg, sz);
    const struct host_node* node = host_node_get(HNODE_ID(NODE_WORLD, 0));
    if (node) {
        UM_SEND(node->connid, fw, UM_forward_size(fw));
    }
}

static inline void
_forward_client(int cid, struct UM_base* um) {
    struct gate_client* c = host_gate_getclient(cid);
    if (c) {
        UM_SENDTOCLI(c->connid, um, um->msgsz);
    }
}

void
forward_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate_message* gm = msg;
    struct gate_client* c = gm->c;
    _forward_world(c, gm->msg, sz);
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
        _forward_client(fw->cid, &fw->wrap);
        break;
        }
    }
}

void
forward_net(struct service* s, struct gate_message* gm) {
    struct net_message* nm = gm->msg;
    struct UM_logout logout;
    switch (nm->type) {
    case NETE_SOCKERR:
        logout.type = LOGOUT_SOCKERR;
        _forward_world(gm->c, &logout, sizeof(logout));
    case NETE_TIMEOUT:
        logout.type = LOGOUT_TIMEOUT;
        _forward_world(gm->c, &logout, sizeof(logout));
        break;
    }
}
