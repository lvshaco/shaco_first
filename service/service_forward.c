#include "host_service.h"
#include "host_gate.h"
#include "host_node.h"
#include "user_message.h"
#include "cli_message.h"
#include "node_type.h"
#include <stdlib.h>
#include <string.h>

static inline void
_forward_world(struct gate_client* c, struct UM_BASE* um) {
    UM_DEFVAR(UM_FORWARD, fw);
    fw->cid = c->connid;
    memcpy(&fw->wrap, um, um->msgsz);
    fw->wrap.nodeid = host_id();
    const struct host_node* node = host_node_get(HNODE_ID(NODE_WORLD, 0));
    if (node) {
        UM_SEND(node->connid, fw, UM_FORWARD_size(fw));
    }
}
void
forward_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gate_message* gm = msg;
    struct gate_client* c = gm->c;
    UM_CAST(UM_BASE, um, gm->msg);
    _forward_world(c, um);
}
void
forward_nodemsg(struct service* s, int id, void* msg, int sz) {
    UM_CAST(UM_BASE, um, msg);
    const struct host_node* node = host_node_get(um->nodeid);
    if (node == NULL)
        return;
    switch (um->msgid) {
    case IDUM_FORWARD: {
        UM_CAST(UM_FORWARD, fw, um);
        struct UM_BASE* m = &fw->wrap;
        struct gate_client* c = host_gate_getclient(fw->cid);
        if (c) {
            if (m->msgid == IDUM_LOGOUT) {
                UM_CAST(UM_LOGOUT, lo, m);
                if (lo->type >= LOGOUT_GATEMAX) {
                    UM_SENDTOCLI(c->connid, m, m->msgsz);
                    host_gate_disconnclient(c, true);
                }
                
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
    UM_DEFFIX(UM_LOGOUT, logout);
    switch (nm->type) {
    case NETE_SOCKERR:
        logout->type = LOGOUT_SOCKERR;
        _forward_world(gm->c, (struct UM_BASE*)logout);
    case NETE_TIMEOUT:
        logout->type = LOGOUT_TIMEOUT;
        _forward_world(gm->c, (struct UM_BASE*)logout);
        break;
    }
}
