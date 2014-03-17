#include "sh.h"
#include "hall_attribute.h"
#include "robot.h"
#include "robot_cmdctl.h"
#include "msg_server.h"
#include "msg_client.h"

static inline struct agent_list *
index_rest_list(struct robot *self, int ai) {
    int idx = ai-1;
    assert(idx >= 0 && idx < AI_MAX);
    return &self->rests[idx];
}

static struct agent *
alloc_agent(struct robot *self) {
    struct agent *ag = malloc(sizeof(*ag));
    memset(ag, 0, sizeof(*ag));
    ag->status = S_REST;
    return ag;
}

static struct agent *
agent_pull(struct robot *self, int ai) {
    struct agent_list *rest = index_rest_list(self, ai);
    
    struct agent *ag = rest->head;
    if (ag == NULL)
        return NULL;
    assert(ag->status == S_REST);
    ag->status = S_WAIT;
    rest->head = rest->head->next;
    return ag;
}

static void
agent_fight(struct robot *self, struct agent *ag) {
    assert(ag->status == S_WAIT);
    ag->status = S_FIGHT;
}

static void
agent_rest(struct robot *self, struct agent *ag) {
    struct agent_list *rest = index_rest_list(self, ag->ai);
    ag->status = S_REST;
    if (rest->head) {
        assert(rest->tail != NULL);
        assert(rest->tail->next == NULL);
        rest->tail->next = ag;
    } else {
        rest->head = ag;
    }
    rest->tail = ag;
    ag->next = NULL;
}

static inline uint32_t
rand_role(struct robot *self) {
    const struct tplt_holder* holder = tplt_get_holder(self->T, TPLT_ROLE);
    if (holder) {
        int n = TPLT_HOLDER_NELEM(holder);
        if (n > 0) {
            int idx = rand()%n;
            const struct role_tplt *tplt = TPLT_HOLDER_FIRSTELEM(role_tplt, holder); 
            return tplt[idx].id;
        }
    }
    return 0;
}

static inline void
init_agent_data(struct module *s, struct agent *ag, int idx, int ai) {
    struct robot *self = MODULE_SELF;
    struct chardata *cdata = &ag->data;
    ag->ai= ai;
    cdata->charid = CHARID_BEGIN+idx;
    snprintf(cdata->name, sizeof(cdata->name), "wabao%02d_%d", ai, cdata->charid);
    cdata->accid = ACCID_BEGIN+idx;
    cdata->role = rand_role(self);
    hall_attribute_main(self->T, cdata);
}

static int
init_agents(struct module *s) {
    struct robot *self = MODULE_SELF;
    sh_hash_init(&self->agents, 1);
    memset(self->rests, 0, sizeof(self->rests));
    // todo
    int count = 100;
    if (count > ROBOT_MAX) {
        count = ROBOT_MAX;
    }
    int i;
    for (i=0; i<count; ++i) {
        struct agent *ag = alloc_agent(self);
        if (ag == NULL) {
            return 1;
        }
        init_agent_data(s, ag, i, rand()%10+1);
        agent_rest(self, ag);
        sh_hash_insert(&self->agents, UID(ag), ag);
    }
    self->nagent = count;
    return 0;
}

static inline void
build_brief(struct agent *ag, struct tmemberbrief *brief) {
    struct chardata *cdata = &ag->data;
    brief->accid = cdata->accid;
    brief->charid = cdata->charid;
    memcpy(brief->name, cdata->name, sizeof(cdata->name));
    brief->level = cdata->level;
    brief->role = cdata->role;
    brief->state = role_state(cdata);
    brief->oxygen = cdata->attri.oxygen;
    brief->body = cdata->attri.body;
    brief->quick = cdata->attri.quick;
}

static inline void
build_detail(struct agent *ag, struct tmemberdetail *detail) {
    struct chardata *cdata = &ag->data;
    detail->accid = cdata->accid;
    detail->charid = cdata->charid;
    memcpy(detail->name, cdata->name, sizeof(cdata->name));
    detail->level = cdata->level;
    detail->role = cdata->role;
    detail->score_dashi = cdata->score_dashi;
    detail->attri = cdata->attri;
}

static void
pull(struct module *s, int source, struct UM_ROBOT_PULL *rp) {
    struct robot *self = MODULE_SELF;
    UM_DEFFIX(UM_ROBOT_APPLY, ra);
    struct agent *ag = agent_pull(self, rp->ai);
    if (ag) {
        ra->info.type = rp->type;
        ra->info.luck_rand = 0;
        ra->info.match_score = rp->match_score;
        ra->info.target.type = rp->target.type;
        ra->info.target.id = rp->target.id;
        build_brief(ag, &ra->info.brief);
        sh_module_send(MODULE_ID, source, MT_UM, ra, sizeof(*ra)); 
        sh_trace("Robot %u pull", UID(ag));
    } else {
        sh_trace("Robot none to pull");
    }
}

static void
play_fail(struct module *s, struct agent *ag) {
    struct robot *self = MODULE_SELF;
    if (ag->status == S_WAIT) { 
        agent_rest(self, ag);
        sh_trace("Robot %u rest, because play fail", UID(ag));
    } else {
        sh_trace("Robot %u rest, because play fail, but status %d", UID(ag), ag->status);
    }
}

static void
enter_room(struct module *s, struct agent *ag, struct UM_ENTERROOM *er) {
    struct robot *self = MODULE_SELF;
    if (ag->status != S_WAIT) {
        sh_trace("Robot %u receive enter room, but status %d", UID(ag), ag->status);
        return;
    }
    if (sh_module_has(self->room_handle, er->room_handle)) {
        agent_fight(self, ag);
        UM_DEFFIX(UM_ROBOT_LOGINROOM, lr);
        lr->roomid = er->roomid;
        lr->ai = ag->ai;
        build_detail(ag, &lr->detail);
        sh_module_send(MODULE_ID, er->room_handle, MT_UM, lr, sizeof(*lr));
        sh_trace("Robot %u send enter room to handle %x", UID(ag), er->room_handle);
    } else {
        agent_rest(self, ag);
        sh_trace("Robot %u receive enter handle %x not exist", UID(ag), er->room_handle);
    }
}

static void
exit_room(struct module *s, uint32_t uid) {
    struct robot *self = MODULE_SELF;
    struct agent *ag = sh_hash_find(&self->agents, uid);
    if (ag == NULL) {
        return;
    }
    if (ag->status == S_FIGHT) {
        agent_rest(self, ag);
        UM_DEFWRAP(UM_MATCH, ma, UM_LOGOUT, lo);
        ma->uid = UID(ag);
        lo->err = SERR_OK;
        sh_module_send(MODULE_ID, self->match_handle, MT_UM, ma, sizeof(*ma)+sizeof(*lo));
        sh_trace("Robot %u notify match exit room", UID(ag));
    } else {
        sh_trace("Robot %u receive exit room, but status %d", UID(ag), ag->status);
    }
}

struct robot*
robot_create() {
    struct robot* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
robot_free(struct robot* self) {
    if (self == NULL)
        return;
    //sh_hash_foreach(&self->agents, free);
    sh_hash_fini(&self->agents);
    memset(self->rests, 0, sizeof(self->rests));
    self->nagent = 0;
    robot_tplt_fini(self);
    free(self);
}

int
robot_init(struct module* s) {
    struct robot* self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    struct sh_monitor_handle h = {MODULE_ID, MODULE_ID};
    if (sh_monitor("match", &h, &self->match_handle) ||
        sh_monitor("room", &h, &self->room_handle)) {
        return 1;
    }
    if (robot_tplt_init(self)) {
        return 1;
    }
    if (init_agents(s)) {
        return 1;
    } 
    return 0;
}

void
robot_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct robot *self = MODULE_SELF;
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ROBOT_PULL: {
            UM_CAST(UM_ROBOT_PULL, rp, msg);
            pull(s, source, rp);
            break;
            }
        case IDUM_MATCH: {
            UM_CAST(UM_MATCH, ma, msg);
            struct agent *ag = sh_hash_find(&self->agents, ma->uid);
            if (ag == NULL) {
                return;
            }
            UM_CAST(UM_BASE, sub, ma->wrap);
            switch (sub->msgid) {
            case IDUM_PLAYFAIL:
                play_fail(s, ag);
                break;
            case IDUM_ENTERROOM: {
                UM_CAST(UM_ENTERROOM, er, ma->wrap);
                enter_room(s, ag, er);
                break;
                }
            }
            break;
            }
        case IDUM_EXITROOM: {
            UM_CAST(UM_EXITROOM, exit, msg);
            exit_room(s, exit->uid);
            break;
            }
        }
        break;
        }
    case MT_CMD:
        cmdctl_handle(s, source, msg, sz, CMDS, -1);
        break;
    }
}
