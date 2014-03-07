#include "sh.h"
#include "msg_server.h"
#include "msg_client.h"

#define WAIT_TIMEOUT 3
#define JOINABLE_TIME 3

#define S_WAITING 0
#define S_CREATING 1
#define S_GAMING 2

#define N_0 0
#define N_1 1
#define N_2 2
#define N_3 3
#define N_4 4
#define N_MAX 5

#define S_MAX 65

static inline uint32_t
match_score_to_N(uint32_t score) {
    if (score < 100000) {
        return N_0;
    } else if (score < 180000) {
        return N_1;
    } else if (score < 250000) {
        return N_2;
    } else if (score < 310000) {
        return N_3;
    } else {
        return N_4;
    }
}

static inline uint32_t
match_score_to_S(uint32_t score) {
    uint32_t slot = score/150;
    return slot < S_MAX ? slot : (S_MAX-1);
}

static inline int
match_score_to_Nai(uint32_t score) {
    static int TO_AI[N_MAX] = {4,5,6,7,8};
    uint32_t slot = match_score_to_N(score);
    return TO_AI[slot]; 
}

static inline int
match_score_to_Sai(uint32_t score) {
    int ai = score / 900 + 1; 
    return ai <= 10 ? ai : 10;
}

static inline uint32_t
match_slot(int type, uint32_t score) {
    if (type == ROOM_TYPE_NORMAL)
        return match_score_to_N(score);
    else
        return match_score_to_S(score);
}

static inline int
match_ai(int type, uint32_t score) {
    if (type == ROOM_TYPE_NORMAL)
        return match_score_to_Nai(score);
    else
        return match_score_to_Sai(score);
}

struct applyer {
    int hall_source; // if is_robot is true, then this is robot_handle
    uint32_t uid;
    bool is_robot;
    int8_t type;
    int8_t status;
    uint8_t luck_rand; 
    uint32_t match_score;
    uint32_t match_slot;
    struct tmemberbrief brief;
    uint32_t roomid;
};

struct member {
    uint32_t uid;
};

struct room {
    uint32_t id;
    uint64_t start_time;
    int room_handle;
    int joinable;
    uint32_t match_score;
    uint32_t match_slot;
    int8_t type;
    int8_t status;
    uint8_t nmember;
    struct member members[MEMBER_MAX];
};

struct waiter {
    bool is_robot;
    uint32_t uid;
    uint64_t waiting_time;
};

struct match {
    int hall_handle;
    int room_handle;
    int robot_handle;
    uint32_t roomid_alloctor;
    uint32_t randseed;
    struct waiter waiting_S[S_MAX];
    struct sh_hash applyers;
    struct sh_hash rooms;
    struct sh_hash joinable_rooms[N_MAX];
};

struct match *
match_create() {
    struct match *self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
match_free(struct match *self) {
    if (self == NULL)
        return;

    sh_hash_foreach(&self->applyers, free);
    sh_hash_fini(&self->applyers);

    sh_hash_foreach(&self->rooms, free);
    sh_hash_fini(&self->rooms);
    int i;
    for (i=0; i<N_MAX; ++i) {
        sh_hash_fini(&self->joinable_rooms[i]);
    }
    free(self);
}

int
match_init(struct module *s) {
    struct match *self = MODULE_SELF;
    if (sh_handle_publish(MODULE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("hall", SUB_REMOTE, &self->hall_handle) ||
        sh_handler("room", SUB_REMOTE, &self->room_handle) ||
        sh_handler("robot", SUB_REMOTE, &self->robot_handle)) {
        return 1;
    }
    self->randseed = sh_timer_now()/1000;
    sh_hash_init(&self->applyers, 1);
    sh_hash_init(&self->rooms, 1);
    int i;
    for (i=0; i<N_MAX; ++i) {
        sh_hash_init(&self->joinable_rooms[i], 1);
    }
    sh_timer_register(MODULE_ID, 1000);
    return 0;
}

static inline uint32_t 
alloc_roomid(struct match *self) {
    uint32_t id = ++self->roomid_alloctor;
    if (id == 0)
        id = ++self->roomid_alloctor;
    return id;
}

static inline struct applyer *
alloc_applyer(struct match *self) {
    struct applyer *ar = malloc(sizeof(*ar));
    return ar;
}

static inline struct room *
alloc_room(struct match *self) {
    struct room *ro = malloc(sizeof(*ro));
    return ro;
}

static inline void
response_play_fail(struct module *s, struct applyer *ar, int err) {
    sh_trace("Match notify %u play fail %d", ar->uid, err);
    UM_DEFWRAP(UM_MATCH, ma, UM_PLAYFAIL, pf);
    ma->uid = ar->uid;
    pf->err = err;
    sh_module_send(MODULE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*pf));
}

static inline void
notify_enter_room(struct module *s, struct applyer *ar, struct room *ro) {
    sh_trace("Match notify %u enter room %u", ar->uid, ro->id);
    UM_DEFWRAP(UM_MATCH, ma, UM_ENTERROOM, enter);
    ma->uid = ar->uid;
    enter->room_handle = ro->room_handle;
    enter->roomid = ro->id;
    sh_module_send(MODULE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*enter));
}

static inline void
notify_destroy_room(struct module *s, struct room *ro) {
    sh_trace("Match notify destroy room %u", ro->id);
    UM_DEFFIX(UM_DESTROYROOM, des);
    des->id = ro->id;
    sh_module_send(MODULE_ID, ro->room_handle, MT_UM, des, sizeof(*des));
}

static inline void
notify_create_room(struct module *s, struct room *ro, struct applyer **as, int n) {
    struct match *self = MODULE_SELF;
    sh_trace("Match notify create room %u", ro->id);
    UM_DEFVAR(UM_CREATEROOM, create);
    create->type = ro->type;
    create->mapid = sh_rand(self->randseed) % 2 + 1; // 1,2 todo
    create->id = ro->id;
    create->max_member = max(2, n); // todo, now 2
    create->nmember = n;
    int i;
    for (i=0; i<n; ++i) {
        create->members[i].is_robot = as[i]->is_robot;
        create->members[i].brief = as[i]->brief;
    }
    sh_module_send(MODULE_ID, ro->room_handle, MT_UM, create, UM_CREATEROOM_size(create));
}

static inline void
notify_join_room(struct module *s, struct room *ro, struct applyer *ar) {
    sh_trace("Match notify join room %u, applyer %u", ro->id, ar->uid);
    UM_DEFFIX(UM_JOINROOM, join);
    join->id = ro->id;
    join->mm.is_robot = ar->is_robot;
    join->mm.brief = ar->brief;
    sh_module_send(MODULE_ID, ro->room_handle, MT_UM, join, sizeof(*join));
}

static inline void
notify_waiting(struct module *s, struct applyer *ar) {
    sh_trace("Match notify waiting %u", ar->uid);
    if (ar->is_robot)
        return;
    UM_DEFWRAP(UM_MATCH, ma, UM_PLAYWAIT, pw);
    ma->uid = ar->uid;
    pw->timeout = WAIT_TIMEOUT;
    sh_module_send(MODULE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*pw));
}

/*
static void
notify_status(struct module *s, struct applyer *ar) {
    switch (ar->status) {
    case S_WAITING:
        notify_waiting(s, ar);
        break;
    case S_CREATING:
        // do noting
        break;
    case S_GAMING: {
        struct room *ro = sh_hash_find(&self->rooms, ar->roomid);
        if (ro) {
            notify_enter_room(s, ar, ro);
        }
        break;
        }
    }
}
*/

static inline void
robot_pull(struct module *s, int type, int match_score, int target_type, int target_id) { 
    struct match *self = MODULE_SELF;
    UM_DEFFIX(UM_ROBOT_PULL, rp);
    rp->type = type;
    rp->ai = match_ai(type, match_score);
    rp->match_score = match_score;
    rp->target.type = target_type;
    rp->target.id = target_id;
    sh_module_send(MODULE_ID, self->robot_handle, MT_UM, rp, sizeof(*rp));
}

static inline struct waiter *
applyer_to_waiter(struct match *self, struct applyer *ar) {
    assert(ar->type == ROOM_TYPE_DASHI);
    assert(ar->match_slot < S_MAX);
    return &self->waiting_S[ar->match_slot];
}

static void
join_waiting(struct match *self, struct applyer *ar) {
    sh_trace("Match new applyer %u join waiting", ar->uid);
    ar->status = S_WAITING;
    struct waiter *one = applyer_to_waiter(self, ar);
    one->is_robot = ar->is_robot;
    one->uid = ar->uid;
    one->waiting_time = sh_timer_now();
}

static void
leave_waiting(struct match *self, struct applyer *ar) {
    struct waiter *one = applyer_to_waiter(self, ar);
    if (one->uid == ar->uid) {
        one->uid = 0;
    }
    sh_trace("Match applyer %u leave waiting %u", ar->uid, one->uid);
}

static struct room * 
create_room(struct match *self, struct applyer *leader, int room_handle) {
    struct room *ro = alloc_room(self);
    uint32_t id = alloc_roomid(self);
    while (sh_hash_find(&self->rooms, id)) { // make sure wrap repeat
        id = alloc_roomid(self);
    }
    ro->id = id;
    ro->start_time = sh_timer_now();
    ro->room_handle = room_handle;
    ro->joinable = false;
    ro->match_score = leader->match_score;
    ro->match_slot = leader->match_slot;
    ro->type = leader->type;
    ro->status = S_CREATING;
    ro->nmember = 0;
    memset(ro->members, 0, sizeof(ro->members));
    return ro;
}

static void
destroy_room(struct match *self, struct room *ro) {
    sh_trace("Match room %u destroy", ro->id); 
    if (ro->joinable) {
        assert(ro->match_slot < N_MAX);
        sh_hash_remove(&self->joinable_rooms[ro->match_slot], ro->id);
    }
    sh_hash_remove(&self->rooms, ro->id);
    free(ro);
}

static void
create_room_ok(struct module *s, uint32_t id) {
    struct match *self = MODULE_SELF;
    sh_trace("Match room %u create ok", id);
    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    ro->status = S_GAMING;

    assert(ro->nmember <= MEMBER_MAX);
    struct applyer *all[MEMBER_MAX];
    int i;
    for (i=0; i<MEMBER_MAX; ++i) {
        if (ro->members[i].uid) {
            all[i] = sh_hash_find(&self->applyers, ro->members[i].uid);
        } else {
            all[i] = NULL;
        }
    }
    for (i=0; i<MEMBER_MAX; ++i) {
        if (all[i]) {
            all[i]->status = S_GAMING;
            notify_enter_room(s, all[i], ro);
        }
    }
}

static void
create_room_fail(struct module *s, uint32_t id, int err) {
    struct match *self = MODULE_SELF;
    sh_trace("Match room %u create fail %d", id, err);
    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    struct applyer *ar;
    int i;
    for (i=0; i<MEMBER_MAX; ++i) {
        if (ro->members[i].uid == 0) {
            continue;
        }
        ar = sh_hash_find(&self->applyers, ro->members[i].uid);
        if (ar == NULL) {
            continue;
        }
        response_play_fail(s, ar, err);
        sh_hash_remove(&self->applyers, ar->uid);
        free(ar);
    }
    destroy_room(self, ro);
}

static inline void
room_switch_joinable(struct match *self, struct room *ro) {
    sh_trace("Match room %u switch to enable join", ro->id);
    assert(!ro->joinable);
    //assert(sh_hash_remove(&self->rooms, ro->id) == ro);
    assert(ro->match_slot < N_MAX);
    assert(!sh_hash_insert(&self->joinable_rooms[ro->match_slot], ro->id, ro));
    ro->joinable = true;
}

static inline void
room_switch_unjoinable(struct match *self, struct room *ro) {
    sh_trace("Match room %u switch to disable join", ro->id);
    assert(ro->joinable);
    //assert(ro->match_slot < N_MAX);
    //assert(sh_hash_remove(&self->joinable_rooms[ro->match_slot], ro->id) == ro);
    //assert(!sh_hash_insert(&self->rooms, ro->id, ro));
    ro->joinable = false;
}

static inline void
add_member(struct room *ro, struct applyer *ar, int i) {
    assert(ro->nmember < MEMBER_MAX);
    assert(i < MEMBER_MAX);
    assert(ro->members[i].uid == 0);
    ar->status = ro->status;
    ar->roomid = ro->id;
    ro->members[i].uid = ar->uid;
    ro->nmember++;
    sh_trace("Match room %u add member %u, cur %u", ro->id, ar->uid, ro->nmember);
}

static inline void
del_member(struct room *ro, struct applyer *ar) {
    assert(ro->nmember > 0);
    int i;
    for (i=0; i<MEMBER_MAX; ++i) {
        if (ro->members[i].uid == ar->uid) {
            ro->members[i].uid = 0;
            ro->nmember--;
            sh_trace("Match room %u del member %u, cur %u", ro->id, ar->uid, ro->nmember);
            return;
        }
    }
    assert(false);
}

static int
join_room(struct module *s, struct room *ro, struct applyer *ar) {
    sh_trace("Match applyer %u join room %u", ar->uid, ro->id);
    assert(ro->nmember < MEMBER_MAX);
    int i;
    for (i=0; i<MEMBER_MAX; ++i) {
        if (ro->members[i].uid == 0) {
            add_member(ro, ar, i);
            notify_join_room(s, ro, ar);
            return 0;
        } 
    }
    return SERR_JOINROOM;
}

static int
leave_room(struct module *s, struct applyer *ar) { 
    struct match *self = MODULE_SELF;
    struct room *ro = sh_hash_find(&self->rooms, ar->roomid);
    if (ro == NULL) {
        return 1;
    }
    sh_trace("Match applyer %u leave room %u", ar->uid, ro->id);
    del_member(ro, ar);
    if (ro->nmember == 0) {
        notify_destroy_room(s, ro);
        destroy_room(self, ro);
    }
    return 0;
}

static int
start_room(struct module *s, struct applyer **as, int n) {
    struct match *self = MODULE_SELF;
    int room_handle = sh_module_minload(self->room_handle);
    if (room_handle == -1) {
        return SERR_NOROOMS;
    }
    struct room *ro = create_room(self, as[0], room_handle);
    sh_trace("Match start room %u, leader %u", ro->id, as[0]->uid);
    int i;
    for (i=0; i<n; ++i) {
        add_member(ro, as[i], i);
    }
    sh_hash_insert(&self->rooms, ro->id, ro);
    notify_create_room(s, ro, as, n);
    return 0;
}

static void
join_room_ok(struct module *s, uint32_t id, uint32_t uid) {
    sh_trace("Match room %u join applyer %u ok", id, uid);
    struct match *self = MODULE_SELF;
    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    struct applyer *me;
    int i;
    for (i=0; i<MEMBER_MAX; ++i) {
        if (ro->members[i].uid != uid) {
            continue;
        }
        me = sh_hash_find(&self->applyers, uid);
        if (me) {
            me->status = S_GAMING;
            notify_enter_room(s, me, ro);
            break;
        }
    } 
}

static void
join_room_fail(struct module *s, uint32_t id, uint32_t uid, int err) {
    sh_trace("Match room %u join applyer %u fail %d", id, uid, err);
    struct match *self = MODULE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar) {
        response_play_fail(s, ar, err); 
        leave_room(s, ar);

        sh_hash_remove(&self->applyers, ar->uid);
        free(ar);
    }
}

static int 
lookup_N(struct module *s, struct applyer *ar) {
    sh_trace("Match applyer %u lookup_N", ar->uid);
    struct match *self = MODULE_SELF;
    assert(ar->match_slot < N_MAX);
    struct room *ro = sh_hash_pop(&self->joinable_rooms[ar->match_slot]);
    if (ro) {
        int r = join_room(s, ro, ar);
        room_switch_unjoinable(self, ro);
        return r;
    } else {
        return start_room(s, &ar, 1); 
    }
}

static int
lookup_S(struct module *s, struct applyer *ar) {
    sh_trace("Match applyer %u lookup_S", ar->uid);
    struct match *self = MODULE_SELF; 
    struct applyer *other = NULL;

    assert(ar->match_slot < S_MAX); 
    struct waiter *one = &self->waiting_S[ar->match_slot];
    if (one->uid) {
        other = sh_hash_find(&self->applyers, one->uid);
        if (other == NULL) {
            one->uid = 0;
        }
    }
    if (other) {
        leave_waiting(self, other);
        struct applyer *as[2] = {ar, other};
        int err = start_room(s, as, 2);
        if (err) {
            join_waiting(self, other);
        }
        return err;
    } else {
        join_waiting(self, ar);
        notify_waiting(s, ar);
        return 0;
    }
}

static int
lookup_room(struct module *s, struct applyer *ar, uint32_t roomid) {
    sh_trace("Match applyer %u lookup_room %u", ar->uid, roomid);
    struct match *self = MODULE_SELF;
    struct room *ro = sh_hash_find(&self->rooms, roomid);
    if (ro == NULL) {
        return SERR_ROOMUNEXIST;
    }
    if (!ro->joinable) {
        return SERR_ROOMUNJOINABLE;
    }
    int r = join_room(s, ro, ar);
    assert(ro->match_slot < N_MAX);
    sh_hash_remove(&self->joinable_rooms[ro->match_slot], roomid);
    room_switch_unjoinable(self, ro);
    return r;
}

static int
lookup(struct module *s, struct applyer *ar, int target_type, int target_id) {
    switch (target_type) {
    case APPLY_TARGET_TYPE_NONE:
        if (ar->type == ROOM_TYPE_NORMAL) {
            return lookup_N(s, ar);
        } else {
            return lookup_S(s, ar);
        }
    case APPLY_TARGET_TYPE_ROOM:
        return lookup_room(s, ar, target_id);
    default:
        return 1;
    }
}

static int
apply(struct module *s, int source, bool is_robot, const struct apply_info *info) {
    struct match *self = MODULE_SELF;

    uint32_t uid = info->brief.accid;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar) {
        //notify_status(s, ar);
        return 1; 
    }
    ar = alloc_applyer(self);
    ar->hall_source = source;
    ar->uid = uid;
    ar->is_robot = is_robot;
    ar->type = info->type;
    ar->status = S_WAITING;
    ar->luck_rand = info->luck_rand; 
    ar->match_score = info->match_score;
    ar->match_slot = match_slot(info->type, info->match_score);
    ar->roomid = 0;
    ar->brief = info->brief;
    int err = lookup(s, ar, info->target.type, info->target.id);
    if (err == 0) {
        assert(!sh_hash_insert(&self->applyers, uid, ar));
        return 0;
    } else {
        sh_trace("Match applyer %u apply fail %d", uid, err);
        response_play_fail(s, ar, err);
        free(ar);
        return 1;
    }
}

static void
player_apply(struct module *s, int source, struct UM_APPLY *ap) {
    sh_trace("Match player %u apply", ap->info.brief.accid);
    apply(s, source, false, &ap->info);
}

static void
robot_apply(struct module *s, int source, struct UM_ROBOT_APPLY *ra) {
    sh_trace("Match robot %u apply", ra->info.brief.accid);
    apply(s, source, true, &ra->info);
}

static void
robot_pull_req(struct module *s, int source, int roomid) {
    struct match *self = MODULE_SELF;
    struct room *ro = sh_hash_find(&self->rooms, roomid);
    if (!ro->joinable) {
        return;
    }
    robot_pull(s, ro->type, ro->match_score, APPLY_TARGET_TYPE_ROOM, roomid);
}

/*
static void
apply_cancel(struct module *s, int source, uint32_t uid) {
    struct match *self = MODULE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar &&
        ar->status == S_WAITING) {
        leave_waiting(self, ar);
        sh_hash_remove(&self->applyers, uid);
        free(ar);
    }
}
*/
static void
logout(struct module *s, uint32_t uid) { 
    struct match *self = MODULE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar == NULL) {
        return;
    }
    sh_trace("Match applyer %u in room %u logout, status %d", ar->uid, ar->roomid, ar->status);
    switch (ar->status) {
    case S_WAITING:
        leave_waiting(self, ar);
        break;
    default:
        leave_room(s, ar);
        break;
    }
    sh_hash_remove(&self->applyers, uid);
    free(ar);
}

void
match_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ROBOT_APPLY: {
            UM_CAST(UM_ROBOT_APPLY, ra, msg);
            robot_apply(s, source, ra);
            break;
        }
        case IDUM_APPLY: {
            UM_CAST(UM_APPLY, ap, msg);
            player_apply(s, source, ap);
            break;
            }
        case IDUM_MATCH: {
            UM_CAST(UM_MATCH, ma, msg);
            UM_CAST(UM_BASE, wrap, ma->wrap);
            switch (wrap->msgid) {
            //case IDUM_APPLYCANCEL: {
                //apply_cancel(s, source, ma->uid);
                //break;
                //}
            case IDUM_LOGOUT: {
                logout(s, ma->uid);
                break;
                }
            }
            break;
            }
        case IDUM_CREATEROOMRES: {
            UM_CAST(UM_CREATEROOMRES, res, msg);
            if (res->err == 0) {
                create_room_ok(s, res->id);
            } else {
                create_room_fail(s, res->id, res->err);
            }
            break;
            }
        case IDUM_JOINROOMRES: {
            UM_CAST(UM_JOINROOMRES, res, msg);
            if (res->err == 0) {
                join_room_ok(s, res->id, res->uid);
            } else {
                join_room_fail(s, res->id, res->uid, res->err);
            }
            break;
            }
        case IDUM_ROBOT_PULL_REQ: {
            UM_CAST(UM_ROBOT_PULL_REQ, req, msg);
            robot_pull_req(s, source, req->roomid);
            break;
            }
        }
        break;
        }
    case MT_MONITOR:
        // todo
        break;
    }
}

static inline void
waiter_time(struct module *s, struct waiter *one) {
    struct match *self = MODULE_SELF;
    if (one->is_robot || one->uid == 0) {
        return;
    }
    if (sh_timer_now() - one->waiting_time < WAIT_TIMEOUT*1000) {
        return;
    }
    sh_trace("Match waiter %u, timeout", one->uid);
    struct applyer *ar = sh_hash_find(&self->applyers, one->uid);
    if (ar) {
        robot_pull(s, ar->type, ar->match_score, APPLY_TARGET_TYPE_NONE, 0); 
    } else {
        one->uid = 0; // sometine wrong
    }
}

static void
roomcb(void *pointer, void *ud) {
    struct module *s = ud;
    struct match *self = MODULE_SELF;
   
    struct room *ro = pointer;
    uint64_t now = sh_timer_now();

    if (ro->type == ROOM_TYPE_NORMAL &&
        ro->status == S_GAMING && 
        !ro->joinable) {
        if (now - ro->start_time > JOINABLE_TIME * 1000) {
            room_switch_joinable(self, ro);
        }
    }
}

void
match_time(struct module *s) {
    struct match *self = MODULE_SELF;
    int i;
    for (i=0; i<S_MAX; ++i) {
        waiter_time(s, &self->waiting_S[i]);
    }
    sh_hash_foreach2(&self->rooms, roomcb, s);
}
