#include "sc_service.h"
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
    uint32_t charid;
    int hall_source;
    //int8_t type;
    int8_t status;
    uint32_t roomid;
    // todo:
};

struct member {
    uint32_t charid;
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
    uint32_t roomid_alloctor;
    uint32_t waiting; // todo, now just simple
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
   
    if (sh_handler("hall", &self->hall_handle) ||
        sh_handler("room", &self->room_handle)) {
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
    UM_DEFWRAP(UM_HALL, ha, UM_PLAYFAIL);
    ha->charid = ar->charid;
    UM_CAST(UM_PLAYFAIL, pf, ha->wrap);
    pf->err = err;
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ha, sizeof(*ha)+sizeof(*pf));
}

static inline void
notify_enter_room(struct service *s, struct applyer *ar, struct room *ro) {
    UM_DEFWRAP(UM_HALL, ha, UM_ENTERROOM);
    ha->charid = ar->charid;

    UM_CAST(UM_ENTERROOM, en, ha->wrap);
    en->room_handle = ro->room_handle;
    en->roomid = ro->id;
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ha, sizeof(*ha)+sizeof(*en));
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
        create->members[i] = ro->members[i].charid;
    }
    sh_service_send(SERVICE_ID, ro->room_handle, MT_UM, create, UM_CREATEROOM_size(create));
}

static inline void
notify_waiting(struct service *s, struct applyer *ar) {
    ar->status = S_WAITING;
    UM_DEFWRAP(UM_HALL, ha, UM_PLAYWAIT);
    ha->charid = ar->charid;
    UM_CAST(UM_PLAYWAIT, pw, ha->wrap);
    pw->timeout = 60; // todo just test
    sh_service_send(SERVICE_ID, ar->hall_source, MT_UM, ha, sizeof(*ha)+sizeof(*pw));
}

static void
room_create_ok(struct service *s, uint32_t id) {
    struct match *self = SERVICE_SELF;

    struct room *ro = sh_hash_find(&self->rooms, id);
    if (ro == NULL) {
        return;
    }
    ro->status = S_GAMING;
 
    struct applyer *ar;
    int i;
    for (i=0; i<ro->nmember; ++i) {
        ar = sh_hash_find(&self->applyers, ro->members[i].charid);
        if (ar) {
            ar->status = S_GAMING;
            notify_enter_room(s, ar, ro);
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
        ar = sh_hash_find(&self->applyers, ro->members[i].charid);
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
    self->waiting = ar->charid;
}

static void
leave_waiting(struct match *self, struct applyer *ar) {
    if (self->waiting == ar->charid) {
        self->waiting = 0;
    }
}

static void
join_room(struct room *ro, struct applyer *ar) {
    assert(ro->nmember < MEMBER_MAX);
    ar->status = S_CREATING;
    ar->roomid = ro->id;
    ro->members[ro->nmember++].charid = ar->charid;
}

static void
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
            response_play_fail(s, ar, SERR_NOROOMS);
            sh_hash_remove(&self->applyers, ar->charid);
            free_applyer(self, ar);
            return;
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
}

static void
apply(struct service *s, int source, uint32_t charid, struct UM_APPLY *ap) {
    struct match *self = SERVICE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, charid);
    if (ar) {
        return; // applyers already
    }
    ar = alloc_applyer(self);
    ar->charid = charid;
    ar->hall_source = source;
    //ar->type = ap->type;
    ar->status = S_WAITING;
    assert(!sh_hash_insert(&self->applyers, charid, ar));

    lookup(s, ar, ap->type);
}

static void
apply_cancel(struct service *s, int source, uint32_t charid) {
    struct match *self = SERVICE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, charid);
    if (ar &&
        ar->status == S_WAITING) {
        leave_waiting(self, ar);
        sh_hash_remove(&self->applyers, charid);
        free_applyer(self, ar);
    }
}

static void
logout(struct service *s, uint32_t charid) {
    struct match *self = SERVICE_SELF;
    struct applyer *ar = sh_hash_find(&self->applyers, charid);
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
    sh_hash_remove(&self->applyers, charid);
    free_applyer(self, ar);
}

void
match_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_HALL: {
            UM_CAST(UM_HALL, ha, msg);
            UM_CAST(UM_BASE, wrap, ha->wrap);
            switch (wrap->msgid) {
            case IDUM_APPLY: {
                UM_CAST(UM_APPLY, ap, msg);
                apply(s, source, ha->charid, ap);
                break;
                }
            case IDUM_APPLYCANCEL: {
                apply_cancel(s, source, ha->charid);
                break;
                }
            case IDUM_LOGOUT: {
                logout(s, ha->charid);
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
