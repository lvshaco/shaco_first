#include "robot_tplt.h"
#include "robot.h"
#include "sh.h"

static int
load_tplt(struct robot *self) {
    tplt_free(self->T);
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ROLE, sizeof(struct role_tplt), 1, TBLFILE(role), 0, TPLT_VIST_VEC32},
    };
    self->T = tplt_create(desc, sizeof(desc)/sizeof(desc[0]));
    return self->T ? 0 : 1;
}

int
robot_tplt_init(struct robot *self) {
    if (load_tplt(self)) {
        return 1;
    }
    return 0;
}

void
robot_tplt_fini(struct robot *self) {
    if (self->T) {
        tplt_free(self->T);
        self->T = NULL;
    }
}

void
robot_tplt_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct robot *self = MODULE_SELF;
    if (type != MT_TEXT)
        return;

    if (!strncmp("reloadres", msg, sz)) {
        if (!load_tplt(self)) {
            sh_info("reloadres tplt ok");
        } else {
            sh_error("reloadres tplt fail");
        }
    }
}
