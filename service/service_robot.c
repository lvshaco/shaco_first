#include "sc_service.h"
#include "sc.h"
#include "sc_log.h"
#include "sc_node.h"
#include "sc_timer.h"
#include "sh_hash.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <string.h>

// Accid  reserve 1000~2000000, create from 2000001
// Charid reserve 1000~2000000, create from 2000001
// Create robot, insert to db
// Load  robot to memory
// Apply robot to match
// Login robot to room
// Award score, exp
// Rank score
// Rank reset
/*
struct agent {
    struct chardata data;
    uint32_t last_change_role_time;
};

struct robot {
    int nrobot;
    struct sh_hash robots;
};

struct robot*
robot_create() {
    struct robot* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
robot_free(struct robot* self) {
    sh_hash_foreach(&self->robots, free);
    sh_hash_fini(&self->robots);
    free(self);
}

int
robot_init(struct service* s) {
    struct robot* self = SERVICE_SELF;
    sh_hash_init(&self->robots, 1);
    return 0;
}

static void
pull(struct service *s, int source, uint8_t count) {
}

void
robot_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    switch (type) {
    case MT_UM: {
        UM_CAST(UM_BASE, base, msg);
        switch (base->msgid) {
        case IDUM_ROBOT_PULL: {
            UM_CAST(UM_ROBOT_PULL, rp, msg);
            pull(s, source, rp->count);
            break;
            }
        }
        break;
        }
    }

}*/
