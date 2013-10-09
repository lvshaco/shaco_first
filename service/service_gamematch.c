#include "host_service.h"
#include "host_node.h"
#include "host_timer.h"
#include "host_dispatcher.h"
#include "worldhelper.h"
#include "sharetype.h"
#include "gfreeid.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct matchtag {
    uint16_t gid;
    uint16_t cid;
    uint32_t charid;
    char name[NAME_MAX];
};

struct playerv {
    int sz;
    struct player* p[MEMBER_MAX];
};

struct memberv {
    int sz;
    struct matchtag p[MEMBER_MAX];
};
struct room {
    int id;
    int used;
    uint64_t createtime;
    int8_t type;
    int load;
    uint16_t sid;
    uint32_t key;
    struct memberv mtag;
};

struct gfroom {
    GFREEID_FIELDS(room);
};

struct gamematch {
    uint32_t key;
    uint32_t create_timeout; 
    struct matchtag mtag;
    struct gfroom creating;
};

static void
_notify_playfail(struct player* p) {
    UM_FORWARD(fw, p->cid, UM_playfail, fail, UMID_PLAYFAIL);
    fail->error = 0;
    UM_SENDTOPLAYER(p, fw);
}
static void
_notify_gameaddr(struct player* p, const struct host_node* hn, uint32_t key) {
    UM_FORWARD(fw, p->cid, UM_notifygame, game, UMID_NOTIFYGAME);
    game->addr = hn->gaddr;
    game->port = hn->gport;
    game->key = key;
    UM_SENDTOPLAYER(p, fw);
}
static uint32_t 
_genkey(struct gamematch* self) {
    return self->key++;
}
static int
_calcload(int8_t type) {
    return 2;
}

static void
_buildmember(struct player* p, struct tmemberbrief* mb) {
    mb->charid = p->data.charid;
    memcpy(mb->name, p->data.name, sizeof(mb->name));
}
static inline void
_build_matchtag(struct player* p, struct matchtag* mtag) {
    mtag->gid = p->gid;
    mtag->cid = p->cid;
    mtag->charid = p->data.charid;
    memcpy(p->data.name, mtag->name, NAME_MAX);
}
static inline void
_clear_matchtag(struct matchtag* mtag) {
    mtag->gid = 0;
    mtag->cid = 0;
    mtag->charid = 0;
    mtag->name[0] = '\0';
}
static void
_getmembers(struct room* r, struct playerv* v) {
    struct memberv* mv = &r->mtag;
    assert(mv->sz <= MEMBER_MAX);
    struct player* p;
    int i;
    for (i=0; i<mv->sz; ++i) {
        p = _getplayer(mv->p[i].gid, mv->p[i].cid);
        if (p && p->data.charid == mv->p[i].charid)
            v->p[i] = p;
        else
            v->p[i] = NULL;
    }
    v->sz = mv->sz;
}
static struct room*
_create_tmproom(struct gamematch* self) {
    struct room* tmproom = GFREEID_ALLOC(room, &self->creating);
    assert(tmproom);
   
    tmproom->createtime = host_timer_now();
    tmproom->key = _genkey(self);
    return tmproom;
}
static void
_destroy_tmproom(struct gamematch* self, struct room* r, int status) {
    struct playerv pv;
    _getmembers(r, &pv);

    struct player* p;
    int i;
    for (i=0; i<pv.sz; ++i) {
        p = pv.p[i];
        if (p) 
            p->status = status;
    }
    const struct host_node* hn = host_node_get(HNODE_ID(NODE_GAME, r->sid));
    switch (status) {
    case PS_GAME:
        if (hn) {
            host_node_updateload(hn->id, -r->load);
        }
        for (i=0; i<pv.sz; ++i) {
            p = pv.p[i];
            if (p) {
                _notify_playfail(p);
            }
        }
        break;
    case PS_ROOM:
        if (hn) {
            for (i=0; i<pv.sz; ++i) {
                p = pv.p[i];
                if (p) {
                    _notify_gameaddr(p, hn, r->key);
                }
            }
        }
        break;
    }
    GFREEID_FREE(room, &self->creating, r);
}
static void
_timeout_tmproom(struct gamematch* self) {
    uint64_t now = host_timer_now();
    struct room* rs = GFREEID_FIRST(&self->creating);
    int i;
    for (i=0; i<GFREEID_CAP(&self->creating); ++i) {
        if (GFREEID_USED(&rs[i])) {
            if (now > rs[i].createtime &&
                now - rs[i].createtime >= self->create_timeout) {
                _destroy_tmproom(self, &rs[i], PS_GAME);
            }
        }
    }
}
static int
_match(struct gamematch* self, struct player* p, struct player* mp, int8_t type) {
    const struct host_node* hn = host_node_minload(NODE_GAME);
    if (hn == NULL) {
        return 1;
    }
    UM_FORWARD(fw, p->cid, UM_playloading, pl, UMID_PLAYLOADING);
    pl->leasttime = 3;
    _buildmember(mp, &pl->member);
    UM_SENDTOPLAYER(p, fw);

    _buildmember(p, &pl->member);
    UM_SENDTOPLAYER(mp, fw);

    struct room* tmproom = _create_tmproom(self); 
    tmproom->type = type;
    tmproom->sid = hn->sid;
    _build_matchtag(p, &tmproom->mtag.p[0]);
    _build_matchtag(mp, &tmproom->mtag.p[1]);

    UM_DEFFIX(UM_createroom, cr, UMID_CREATEROOM);
    cr.type = type;
    cr.id = GFREEID_ID(tmproom, &self->creating);
    cr.key = tmproom->key;
    UM_SENDTONODE(hn, &cr, sizeof(cr));
    host_node_updateload(hn->id, _calcload(cr.type));
    return 0;
}
static int
_lookup(struct gamematch* self, struct player* p, int8_t type) {
    struct matchtag* mtag = &self->mtag;
    struct player* mp = _getplayerbyid(mtag->charid);
    if (mp == NULL) {
        p->status = PS_WAITING;
        _build_matchtag(p, mtag);

        UM_DEFFIX(UM_playwait, pw, UMID_PLAYWAIT); 
        pw.timeout = 60;
        UM_SENDTONID(NODE_GATE, p->gid, &pw, sizeof(pw));
        return 0;
    } else {
        _clear_matchtag(mtag);
        if (_match(self, p, mp, type)) {
            _notify_playfail(p);
            _notify_playfail(mp);
            p->status = PS_GAME;
            mp->status = PS_GAME;

        } else {
            p->status = PS_CREATING;
            mp->status = PS_CREATING;
        }
        return 0;
    }
}

static void
_onoverroom(struct gamematch* self, struct node_message* nm) {
    UM_CAST(UM_overroom, or, nm->um);
    int load = _calcload(or->type);
    host_node_updateload(nm->hn->id, -load);
}

static void
_oncreateroom(struct gamematch* self, struct node_message* nm) {
    UM_CAST(UM_createroomres, res, nm->um);
   
    struct room* tmproom = GFREEID_SLOT(&self->creating, res->id);
    if (tmproom || !GFREEID_USED(tmproom)) {
        return;
    }
    if (tmproom->key == res->key &&
        tmproom->sid == nm->hn->sid) {
        _destroy_tmproom(self, tmproom, res->ok ? PS_ROOM : PS_GAME);
    }
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
    SUBSCRIBE_MSG(s->serviceid, UMID_CREATEROOMRES);
    SUBSCRIBE_MSG(s->serviceid, UMID_OVERROOM);
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
    case UMID_OVERROOM:
        _onoverroom(self, nm);
        break;
    case UMID_CREATEROOMRES:
        _oncreateroom(self, nm);
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
    struct gamematch* self= SERVICE_SELF;
    _timeout_tmproom(self);
}
