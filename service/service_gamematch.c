#include "host_service.h"
#include "host_dispatcher.h"
#include "worldhelper.h"
#include "sharetype.h"
#include <stdlib.h>
#include <string.h>

struct matchplayer {
    uint32_t charid;
    char name[NAME_MAX];
};
struct creating {
    int cap;
    int size;
};

struct gamematch {
    uint32_t key;
    struct matchplayer p;
};
static int
_calcload(int8_t type) {
    return 2;
}

static void
_buildmember(struct player* p, struct tmember_brief* mb) {
    mb->charid = p->data.charid;
    memcpy(mb->name, p->data.name, sizeof(mb->name));
}

static int
_match(struct gamematch* self, struct player* p, struct player* p2) {
    const struct host_node* hn = host_node_minload(NODE_GAME);
    if (hn == NULL) {
        return 1;
    }
    UM_FORWARD(fw, p->cid, UM_playloading, pl, UMID_PLAYLOADING);
    pl->leasttime = 3;
    _buildmember(p2, &pl->member);
    UM_SENDTOPLAYER(p, fw);

    _buildmember(p, &pl->member);
    UM_SENDTOPLAYER(p2, fw);

    UM_DEFFIX(UM_createroom, cr, UMID_CREATEROOM);
    cr.type = ROOM_TYPE1;
    cr.key = self->key++;
    UM_SENDTONODE(hn, &cr, sizeof(cr));
    host_node_updateload(hn->id, _calcload(cr.type));
    return 0;
}

static inline void
_buildmatchplayer(struct player* p, struct matchplayer* mp) {
    mp->charid = p->data.charid;
    memcpy(mp->name, p->data.name, NAME_MAX);
}

static int
_lookup(struct gamematch* self, struct player* p, int8_t type) {
    struct matchplayer* mp = &self->p;
    struct player* p2 = _getplayerbyid(mp->charid);
    if (p2 == NULL) {
        _buildmatchplayer(p, mp);

        UM_DEFFIX(UM_playwait, pw, UMID_PLAYWAIT); 
        pw.timeout = 60;
        UM_SENDTONID(NODE_GATE, p->gid, &pw, sizeof(pw));
        return 0;
    } else {
        return _match(self, p, p2);
    }
}

static void
_ondestroyroom(struct gamematch* self, struct node_message* nm) {
    UM_CAST(UM_destroyroom, dr, nm->um);
    int load = _calcload(dr->type);
    host_node_updateload(nm->hn->id, -load);
}

static void
_playreq(struct gamematch* self, struct player_message* pm) {
    UM_CAST(UM_play, um, pm->um);
    _lookup(self, pm->pl, um->type);
}

struct gamematch*
gamematch_create() {
    struct gamematch* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gamematch_free(struct gamematch* self) {
    free(self);
}

int
gamematch_init(struct service* s) {
    SUBSCRIBE_MSG(s->serviceid, UMID_PLAY);
    SUBSCRIBE_MSG(s->serviceid, UMID_DESTORYROOM);
    return 0;
}

void
gamematch_service(struct service* s, struct service_message* sm) {
    //struct gamematch* self = SERVICE_SELF;
}

static void
_handlegate(struct gamematch* self, struct node_message* nm) {
    struct player_message pm;
    if (_decode_playermessage(nm, &pm)) {
        return;
    }
    switch (pm.um->msgid) {
    case UMID_PLAY:
        _playreq(self, &pm);
        break;
    }
}

static void
_handlegame(struct gamematch* self, struct node_message* nm) {
    switch (nm->um->msgid) {
    case UMID_DESTORYROOM:
        _ondestroyroom(self, nm);
        break;
    }
}

void
gamematch_nodemsg(struct service* s, int id, void* msg, int sz) {
    struct gamematch* self = SERVICE_SELF;
    struct node_message nm;
    if (_decode_nodemessage(msg, sz, &nm)) {
        return;
    }
    switch (nm.hn->tid) {
    case NODE_GATE:
        _handlegate(self, &nm);
        break;
    case NODE_GAME:
        _handlegame(self, &nm);
        break;
    }
}

void
gamematch_time(struct service* s) {
    //struct gamematch* self= SERVICE_SELF;
}
