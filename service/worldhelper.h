#ifndef __worldhelper_h__
#define __worldhelper_h__

#include "player.h"
#include "message.h"
#include "host_node.h"
#include "host_log.h"
#include "node_type.h"
#include "user_message.h"
#include "cli_message.h"

struct player_message {
    const struct host_node* hn;
    struct UM_base* um;
    struct player* pl;
};

static void
_notify_logout(const struct host_node* node, int cid, int8_t type) {
    UM_FORWARD(fw, cid, UM_logout, lo, UMID_LOGOUT);
    lo->type = LOGOUT_RELOGIN;
    UM_SENDFORWARD(node->connid, fw);
}

static inline int
_decode_playermessage(struct node_message* nm, struct player_message* pm) {
    const struct host_node* hn = nm->hn;
    struct player* pl;
    UM_CAST(UM_forward, fw, nm->um);

    pl = _getplayer(hn->sid, fw->cid);
    if (pl == NULL) {
        _notify_logout(hn, fw->cid, LOGOUT_NOLOGIN);
        return 1;
    }
    pm->hn = hn; 
    pm->um = &fw->wrap;
    pm->pl = pl;
    return 0;
}

static inline void
UM_SENDTOPLAYER(struct player* pl, struct UM_forward* fw) {
    const struct host_node* hn = host_node_get(HNODE_ID(NODE_GATE, pl->gid));
    if (hn) {
        UM_SENDFORWARD(hn->connid, fw);
    }
}

#endif
