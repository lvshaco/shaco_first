#include "sc_service.h"
#include "sc_timer.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sh_hash.h"
#include "user_message.h"
#include "cli_message.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define S_WAITING 0
#define S_CREATING 1
#define S_GAMING 2

struct applyer {
    uint32_t uid;
    int hall_source; // if isrobot is true, then this is robot_handle
    int8_t status;
    bool isrobot;
    uint32_t roomid;

    struct tmemberbrief brief;
};

struct member {
    uint32_t uid;
};

struct room {
    int id;
    int room_handle;
    int8_t type;
    int8_t status;
    uint8_t nlogin;
    uint8_t nmember;
    struct member members[MEMBER_MAX];
};

struct match {
    int hall_handle;
    int room_handle;
    int robot_handle;
    uint32_t roomid_alloctor;
    uint32_t waiting; // todo, now just simple
    uint64_t wait_time;
    bool isrobot_wait;
    struct sh_hash applyers;
    struct sh_hash rooms;
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
    sh_hash_fini(&self->applyers);
    sh_hash_fini(&self->rooms);
    free(self);
}

int
match_init(struct service *s) {
    struct match *self = SERVICE_SELF;
    if (sh_handle_publish(SERVICE_NAME, PUB_SER)) {
        return 1;
    }
    if (sh_handler("hall", SUB_REMOTE, &self->hall_handle) ||
        sh_handler("room", SUB_REMOTE, &self->room_handle) ||
        sh_handler("robot", SUB_REMOTE, &self->robot_handle)) {
        return 1;
    }
    sh_hash_init(&self->applyers, 1);
    sh_hash_init(&self->rooms, 1);
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

static inline void
free_applyer(struct match *self, struct applyer *ar) {
    free(ar);
}

static inline struct room *
alloc_room(struct match *self) {
    struct room *ro = malloc(sizeof(*ro));
    return ro;
}

static inline void
free_room(struct match *self, struct room *ro) {
    free(ro);
}

static struct room * 
room_create(struct match *self, int8_t type, int room_handle) {
    struct room *ro = alloc_room(self);
    uint32_t id = alloc_roomid(self);
    while (sh_hash_find(&self->rooms, id)) { // make sure wrap repeat
        id = alloc_roomid(self);
    }
    ro->id = id;
    ro->room_handle = room_handle;
    ro->type = type;
    ro->status = S_CREATING;
    ro->nlogin = 0;
    ro->nmember = 0;
    return ro;
}

static inline void
response_play_fail(struct service *s, struct applyer *ar, int err) {
    UM_DEFWRAP(UM_MATCH, ma, UM_PLAYFAIL, pf);
    ma->uid = ar->uid;
    pf->err = err;
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*pf));
}

static inline void
notify_enter_room(struct service *s, struct applyer *ar, struct room *ro) {
    UM_DEFWRAP(UM_MATCH, ma, UM_ENTERROOM, enter);
    ma->uid = ar->uid;
    enter->room_handle = ro->room_handle;
    enter->roomid = ro->id;
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*enter));
}

static inline void
notify_destroy_room(struct service *s, struct room *ro) {
    UM_DEFFIX(UM_DESTROYROOM, des);
    des->id = ro->id;
    sh_service_send(SERVICE_ID, ro->room_handle, MT_UM, des, sizeof(*des));
}

static inline void
notify_create_room(struct service *s, struct room *ro) {
    UM_DEFVAR(UM_CREATEROOM, create);
    create->type = ro->type;
    create->mapid = 1;//sc_rand(self->randseed) % 2 + 1; // 1,2 todo
    create->id = ro->id;
    create->nmember = ro->nmember;
    int i;
    for (i=0; i<ro->nmember; ++i) {
        create->members[i] = ro->members[i].uid;
    }
    sh_service_send(SERVICE_ID, ro->room_handle, MT_UM, create, UM_CREATEROOM_size(create));
}

static inline void
notify_waiting(struct service *s, struct applyer *ar) {
    UM_DEFWRAP(UM_MATCH, ma, UM_PLAYWAIT, pw);
    ma->uid = ar->uid;
    pw->timeout = 60; // todo just test
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*pw));
}

static inline void
notify_loading(struct service *s, struct applyer *ar, struct applyer *other) {
    UM_DEFWRAP(UM_MATCH, ma, UM_PLAYLOADING, pl);
    ma->uid = ar->uid;
    pl->leasttime = ROOM_LOAD_TIMELEAST;
    if (other) {
        pl->member = other->brief;
    } else {
        // maybe other == NULL, if other logout
        memset(&pl->member, 0, sizeof(pl->member));
    }
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ma, sizeof(*ma)+sizeof(*pl));
}

static void
room_create_ok(struct service *s, uint32_t id) {
    struct match *self = SERVICE_SELF;

    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    ro->status = S_GAMING;

    assert(ro->nmember <= MEMBER_MAX);
    struct applyer *all[MEMBER_MAX];
    memset(all, 0, sizeof(all));

    int i;
    for (i=0; i<ro->nmember; ++i) {
        all[i] = sh_hash_find(&self->applyers, ro->members[i].uid);
        if (all[i]) {
            all[i]->status = S_GAMING;
            notify_enter_room(s, all[i], ro);
        }
    }
    for (i=0; i<ro->nmember; ++i) {
        if (all[i]) {
            all[i]->status = S_GAMING;
            notify_enter_room(s, all[i], ro);
            notify_loading(s, all[i], all[ro->nmember-1-i]);
        }
    }
}

static void
room_create_fail(struct service *s, uint32_t id, int err) {
    struct match *self = SERVICE_SELF;

    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    struct applyer *ar;
    int i;
    for (i=0; i<ro->nmember; ++i) {
        ar = sh_hash_find(&self->applyers, ro->members[i].uid);
        if (ar) {
            response_play_fail(s, ar, err);
            free_applyer(self, ar);
        }
    }
    sh_hash_remove(&self->rooms, id);
    free_room(self, ro);
}

static void
join_waiting(struct match *self, struct applyer *ar) {
    ar->status = S_WAITING;
    self->waiting = ar->uid;
    self->wait_time = sc_timer_now();
    self->isrobot_wait = ar->isrobot;
}

static void
leave_waiting(struct match *self, struct applyer *ar) {
    if (self->waiting == ar->uid) {
        self->waiting = 0;
    }
}

static void
join_room(struct room *ro, struct applyer *ar) {
    assert(ro->nmember < MEMBER_MAX);
    ar->status = S_CREATING;
    ar->roomid = ro->id;
    ro->members[ro->nmember++].uid = ar->uid;
}

static int
lookup(struct service *s, struct applyer *ar, int8_t type) {
    struct match *self = SERVICE_SELF;

    struct applyer *other = NULL;  
    if (self->waiting > 0) {
        other = sh_hash_find(&self->applyers, self->waiting);
        if (other == NULL) {
            self->waiting = 0; // something wrong
        }
    }
    if (other) {
        int room_handle = sc_service_minload(self->room_handle);
        if (room_handle == -1) {
            return 1;
        }
        struct room *ro = room_create(self, type, room_handle);
        join_room(ro, ar);
        join_room(ro, other);
        sh_hash_insert(&self->rooms, ro->id, ro);
        notify_create_room(s, ro);
    } else { 
        join_waiting(self, ar);
        notify_waiting(s, ar);
    }
    return 0;
}

static int
apply(struct service *s, int source, bool isrobot, int type, 
        const struct tmemberbrief *brief) {
    struct match *self = SERVICE_SELF;

    uint32_t uid = brief->accid;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar) {
        return 1; // applyers already
    }
    ar = alloc_applyer(self);
    ar->uid = uid;
    ar->hall_source = source;
    //ar->type = ap->type;
    ar->status = S_WAITING;
    ar->isrobot = isrobot;
    ar->roomid = 0;
    ar->brief = *brief;
    if (!lookup(s, ar, type)) {
        assert(!sh_hash_insert(&self->applyers, uid, ar));
        return 0;
    } else {
        response_play_fail(s, ar, SERR_NOROOMS);
        free_applyer(self, ar);
        return 1;
    }
}

static void
player_apply(struct service *s, int source, struct UM_APPLY *ap) {
    apply(s, source, false, ap->type, &ap->brief);
}

static void
robot_apply(struct service *s, int source, struct UM_ROBOT_APPLY *ra) {
    apply(s, source, true, 0, &ra->brief);
}

static void
apply_cancel(struct service *s, int source, uint32_t uid) {
    struct match *self = SERVICE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar &&
        ar->status == S_WAITING) {
        leave_waiting(self, ar);
        sh_hash_remove(&self->applyers, uid);
        free_applyer(self, ar);
    }
}

static void
logout(struct service *s, uint32_t uid) {
    struct match *self = SERVICE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, uid);
    if (ar == NULL) {
        return;
    }
    if (ar->status != S_WAITING) {
        struct room *ro = sh_hash_find(&self->rooms, ar->roomid);
        if (ro) {
            ro->nlogin--;
            if (ro->nlogin == 0) {
                notify_destroy_room(s, ro);                
                sh_hash_remove(&self->rooms, ro->id);
                free_room(self, ro);
            }
        }
    }
    sh_hash_remove(&self->applyers, uid);
    free_applyer(self, ar);
}

void
match_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
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
            case IDUM_APPLYCANCEL: {
                apply_cancel(s, source, ma->uid);
                break;
                }
            case IDUM_LOGOUT: {
                logout(s, ma->uid);
                break;
                }
            }
            break;
            }
        case IDUM_CREATEROOMRES: {
            UM_CAST(UM_CREATEROOMRES, result, msg);
            if (result->err == 0) {
                room_create_ok(s, result->id);
            } else {
                room_create_fail(s, result->id, result->err);
            }
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

void
match_time(struct service *s) {
    struct match *self = SERVICE_SELF;
    if (self->waiting != 0 && !self->isrobot_wait) {
        if (self->wait_time - sc_timer_now() > 15*1000) {
            UM_DEFFIX(UM_ROBOT_PULL, rp);
            rp->count = 1;
            sh_service_send(SERVICE_ID, self->robot_handle, MT_UM, rp, sizeof(*rp));
        }
    }
}
