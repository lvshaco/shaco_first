#ifndef __worldhelper_h__
#define __worldhelper_h__

#include "message.h"
#include "host_node.h"
#include "node_type.h"
#include "user_message.h"
#include "cli_message.h"

// bind player to msg
struct player_message {
    _NODEM_header;
    struct player* p;
};

static inline void
_forward_toplayer(struct player* p, struct UM_FORWARD* fw) {
    const struct host_node* n = host_node_get(HNODE_ID(NODE_GATE, p->gid));
    if (n) {
        UM_SENDFORWARD(n->connid, fw);
    }
}

static inline void
_forward_logout(struct player* p, int32_t error) {
    UM_DEFFORWARD(fw, p->cid, UM_LOGOUT, lo);
    lo->error = error;
    _forward_toplayer(p, fw);
}

static inline void
_forward_loginfail(struct player* p, int32_t error) {
    UM_DEFFORWARD(fw, p->cid, UM_LOGINFAIL, lf);
    lf->error = error;
    _forward_toplayer(p, fw);
}

static inline int
_decode_playermessage(struct node_message* nm, struct player_message* pm) {
    const struct host_node* hn = nm->hn;
    UM_CAST(UM_FORWARD, fw, nm->um);
    struct player* p = _getplayer(hn->sid, fw->cid);
    if (p == NULL) {
        UM_DEFFORWARD(fw, p->cid, UM_LOGOUT, lo);
        lo->error = SERR_NOLOGIN;
        UM_SENDFORWARD(hn->connid, fw);
        return 1;
    }
    pm->hn = hn; 
    pm->um = &fw->wrap;
    pm->p = p;
    return 0;
}

#endif
