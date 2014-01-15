#include "sc_service.h"
#include "sh_util.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "sc_dispatcher.h"
#include "player.h"
#include "worldhelper.h"
#include "sharetype.h"
#include "gfreeid.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define CREATE_TIMEOUT 5000

struct matchtag {
    uint16_t gid;
    uint16_t cid;
    uint32_t charid;
    char name[CHAR_NAME_MAX];
};

struct playerv {
    int np;
    struct player* p[MEMBER_MAX];
};

struct memberv {
    int np;
    struct matchtag p[MEMBER_MAX];
};
struct room {
    int id;
    int used;
    uint64_t createtime;
    int8_t type;
    uint16_t sid;
    int roomid; 
    uint32_t key;
    struct memberv mtag;
};

struct gfroom {
    GFREEID_FIELDS(room);
};

struct gamematch {
    int award_handler;
    uint32_t randseed;
    uint32_t key;
    struct matchtag mtag;
    struct gfroom creating;
};

struct gamematch*
gamematch_create() {
    struct gamematch* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
gamematch_free(struct gamematch* self) {
    if (self == NULL)
        return;
    GFREEID_FINI(room, &self->creating);
    free(self);
}

int
gamematch_init(struct service* s) {
    struct gamematch* self = SERVICE_SELF;
    if (sh_handler("awardlogic", &self->award_handler))
        return 1;

    self->randseed = time(NULL);

    // todo test this
    GFREEID_INIT(room, &self->creating, 1);

    SUBSCRIBE_MSG(s->serviceid, IDUM_PLAY);
    SUBSCRIBE_MSG(s->serviceid, IDUM_LOGOUT);
    SUBSCRIBE_MSG(s->serviceid, IDUM_CREATEROOMRES);
    SUBSCRIBE_MSG(s->serviceid, IDUM_OVERROOM);

    sc_timer_register(s->serviceid, 1000);
    return 0;
}

static void
_notify_playfail(struct player* p, int8_t error) {
    UM_DEFFORWARD(fw, p->cid, UM_PLAYFAIL, fail);
    fail->error = error;
    _forward_toplayer(p, fw);
}

static void
_notify_gameaddr(struct player* p, const struct sc_node* hn, struct room* ro) {
    UM_DEFFORWARD(fw, p->cid, UM_NOTIFYGAME, game);
    game->addr = hn->gaddr;
    game->port = hn->gport;
    game->key = ro->key;
    game->roomid = ro->roomid;
    _forward_toplayer(p, fw);
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
_build_memberbrief(const struct player* p, struct tmemberbrief* brief) {
    const struct chardata* data = &p->data;
    brief->charid = data->charid;
    strncpychk(brief->name, sizeof(brief->name), data->name, sizeof(data->name));
    brief->role = data->role;
    brief->skin = data->skin;
    brief->oxygen = data->attri.oxygen;
    brief->body = data->attri.body;
    brief->quick = data->attri.quick;
}

static void
_build_memberdetail(const struct player* p, struct tmemberdetail* detail) {
    memset(detail, 0, sizeof(*detail));
    const struct chardata* data = &p->data;
    detail->charid = data->charid;
    strncpychk(detail->name, sizeof(detail->name), data->name, sizeof(data->name));
    detail->role = data->role;
    detail->skin = data->skin;
    detail->score_dashi = data->score_dashi;
    detail->attri = data->attri;
}

static inline void
_build_matchtag(const struct player* p, struct matchtag* mtag) {
    mtag->gid = p->gid;
    mtag->cid = p->cid;
    mtag->charid = p->data.charid;
    strncpychk(mtag->name, sizeof(mtag->name), p->data.name, sizeof(p->data.name));
}

static inline void
_clear_matchtag(struct matchtag* mtag) {
    mtag->gid = 0;
    mtag->cid = 0;
    mtag->charid = 0;
    mtag->name[0] = '\0';
}

static void
_del_waitmember(struct gamematch* self, struct player* p) {
    if (p->data.charid == self->mtag.charid) {
        _clear_matchtag(&self->mtag);
    }
}

static void
_get_tmpmembers(struct room* ro, struct playerv* v) {
    struct memberv* mv = &ro->mtag;
    assert(mv->np <= MEMBER_MAX);
    struct player* p;
    int i;
    for (i=0; i<mv->np; ++i) {
        p = _getplayer(mv->p[i].gid, mv->p[i].cid);
        if (p && p->data.charid == mv->p[i].charid)
            v->p[i] = p;
        else
            v->p[i] = NULL;
    }
    v->np = mv->np;
}

static void
_del_tmpmember(struct gamematch* self, struct player* p) {
    struct room* ro = GFREEID_SLOT(&self->creating, p->roomid);
    if (ro == NULL) {
        return;
    }
    struct matchtag* mtag;
    int i;
    for (i=0; i<ro->mtag.np; ++i) {
        mtag = &ro->mtag.p[i];
        if (mtag->charid == p->data.charid) {
            _clear_matchtag(mtag);
        }
    } 
}

static struct room*
_create_tmproom(struct gamematch* self) {
    struct room* ro = GFREEID_ALLOC(room, &self->creating);
    assert(ro);
   
    ro->createtime = sc_timer_now();
    ro->key = _genkey(self);
    return ro;
}

static int
_destroy_tmproom(struct gamematch* self, struct room* ro, int err) {
    const struct sc_node* node;
    struct playerv pv;
    _get_tmpmembers(ro, &pv);

    int status = err == SERR_OK ? PS_ROOM : PS_GAME;
    struct player* p;
    int i;
    for (i=0; i<pv.np; ++i) {
        p = pv.p[i];
        if (p) 
            p->status = status;
    }
    if (err != SERR_OK) {
        for (i=0; i<pv.np; ++i) {
            p = pv.p[i];
            if (p) {
                _notify_playfail(p, err);
            }
        }
    }
    int nodeid = HNODE_ID(NODE_GAME, ro->sid);
    node = sc_node_get(nodeid);
    if (node) {
        if (err == SERR_OK) {
            for (i=0; i<pv.np; ++i) {
                p = pv.p[i];
                if (p) {
                    _notify_gameaddr(p, node, ro);
                }
            }
        } else {
            int load = _calcload(ro->type);
            sc_node_updateload(node->id, -load);
        }
    }
    GFREEID_FREE(room, &self->creating, ro);
    return node ? 0 : 1;
}

static void
_timeout_tmproom(struct gamematch* self) {
    uint64_t now = sc_timer_now();
    struct room* rs = GFREEID_FIRST(&self->creating);
    int i;
    for (i=0; i<GFREEID_CAP(&self->creating); ++i) {
        if (GFREEID_USED(&rs[i])) {
            if (now > rs[i].createtime &&
                now - rs[i].createtime >= CREATE_TIMEOUT) {
                _destroy_tmproom(self, &rs[i], SERR_OK);
            }
        }
    }
}

static int
_match(struct gamematch* self, struct player* p, struct player* mp, int8_t type) {
    const struct sc_node* hn = sc_node_minload(NODE_GAME);
    if (hn == NULL) {
        return 1;
    }
    UM_DEFFORWARD(fw, p->cid, UM_PLAYLOADING, pl);
    pl->leasttime = ROOM_LOAD_TIMELEAST;
    _build_memberbrief(mp, &pl->member);
    _forward_toplayer(p, fw);

    fw->cid = mp->cid;
    _build_memberbrief(p, &pl->member);
    _forward_toplayer(mp, fw);

    struct room* ro = _create_tmproom(self); 
    ro->type = type;
    ro->sid = hn->sid; 
    _build_matchtag(p, &ro->mtag.p[0]);
    _build_matchtag(mp, &ro->mtag.p[1]);
    ro->mtag.np = 2;

    UM_DEFVAR(UM_CREATEROOM, cr);
    cr->type = type;
    cr->mapid = 1;//sc_rand(self->randseed) % 2 + 1; // 1,2 todo
    cr->id = GFREEID_ID(ro, &self->creating);
    cr->key = ro->key;
    _build_memberdetail(p, &cr->members[0]);
    _build_memberdetail(mp, &cr->members[1]);
    cr->nmember = ro->mtag.np;

    UM_SENDTONODE(hn, cr, UM_CREATEROOM_size(cr));
    sc_node_updateload(hn->id, _calcload(cr->type));
    return 0;
}

static int
_lookup(struct gamematch* self, struct player* p, int8_t type) {
    struct matchtag* mtag = &self->mtag;
    struct player* mp = _getplayerbycharid(mtag->charid);
    if (mp == NULL) {
        p->status = PS_WAITING;
        _build_matchtag(p, mtag);

        UM_DEFFORWARD(fw, p->cid, UM_PLAYWAIT, pw);
        pw->timeout = 60; // todo just test
        _forward_toplayer(p, fw);
        return 0;
    } else {
        _clear_matchtag(mtag);
        if (_match(self, p, mp, type)) {
            _notify_playfail(p, 0);
            _notify_playfail(mp, 0);
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
    UM_CAST(UM_OVERROOM, or, nm->um);
    int load = _calcload(or->type);
    sc_node_updateload(nm->hn->id, -load);

    struct player* allp[MEMBER_MAX];
    struct player* p;
    int i, n = 0;
    for (i=0; i<min(MEMBER_MAX, or->nmember); ++i) {
        p = _getplayerbycharid(or->awards[i].charid);
        if (p == NULL)
            continue;
        allp[n++] = p;
    }
    struct service_message sm;
    sm.p1 = allp;
    sm.p2 = or->awards;
    sm.i1 = n;
    sm.i2 = or->type;
    service_notify_service(self->award_handler, &sm);
}

static void
_oncreateroom(struct gamematch* self, struct node_message* nm) {
    UM_CAST(UM_CREATEROOMRES, res, nm->um);
   
    struct room* ro = GFREEID_SLOT(&self->creating, res->id);
    if (ro) {
        if (ro->key == res->key &&
            ro->sid == nm->hn->sid) {
            ro->roomid = res->roomid;
            if (_destroy_tmproom(self, ro, res->error) == 0)
                return;
        }
    }
    //if (res->error) {
        //res->ok = 0;
        //UM_SENDTONODE(nm->hn, res, res->msgsz);
    //}
}

static void
_playreq(struct gamematch* self, struct player_message* pm) {
    UM_CAST(UM_PLAY, um, pm->um);
    _lookup(self, pm->p, um->type);
}

static void
_logout(struct gamematch* self, struct player_message* pm) {
    struct player* p = pm->p;
    switch (p->status) {
    case PS_WAITING:
        _del_waitmember(self, p);
        break;
    case PS_CREATING:
        _del_tmpmember(self, p);
        break;
    }
}

static void
_handlegame(struct gamematch* self, struct node_message* nm) {
    switch (nm->um->msgid) {
    case IDUM_OVERROOM:
        _onoverroom(self, nm);
        break;
    case IDUM_CREATEROOMRES:
        _oncreateroom(self, nm);
        break;
    }
}

void
gamematch_usermsg(struct service* s, int id, void* msg, int sz) {
    struct gamematch* self = SERVICE_SELF;
    struct player_message* pm = msg;
    switch (pm->um->msgid) {
    case IDUM_PLAY:
        _playreq(self, pm);
        break;
    case IDUM_LOGOUT:
        _logout(self, pm);
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
