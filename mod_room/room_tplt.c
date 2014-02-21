#include "room_tplt.h"
#include "mapdatamgr.h"
#include "room.h"
#include "sh.h"

static int
load_tplt(struct room *self) {
    tplt_free(self->T);
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ITEM, sizeof(struct item_tplt), 1, TBLFILE(item), 0, TPLT_VIST_VEC32},
        { TPLT_MAP,  sizeof(struct map_tplt),  1, TBLFILE(map),  0, TPLT_VIST_VEC32},
    };
    self->T = tplt_create(desc, sizeof(desc)/sizeof(desc[0]));
    return self->T ? 0 : 1;
}

static int
load_mapdata(struct room *self) {
    mapdatamgr_fini(self->MH);
    self->MH = mapdatamgr_init(self->T, "./res/map");
    return self->MH ? 0 : 1;
}

int
room_tplt_init(struct room *self) {
    if (load_tplt(self)) {
        return 1;
    }
    if (load_mapdata(self)) {
        return 1; 
    }
    return 0;
}

void
room_tplt_fini(struct room *self) {
    if (self->MH) {
        mapdatamgr_fini(self->MH);
        self->MH = NULL;
    }
    if (self->T) {
        tplt_free(self->T);
        self->T = NULL;
    }
}

void
room_tplt_main(struct module *s, int session, int source, int type, const void *msg, int sz) {
    struct room *self = MODULE_SELF;
    if (type != MT_TEXT)
        return;

    if (!strncmp("reload", msg, sz)) {
        if (!load_tplt(self)) {
            sh_info("reload tplt ok");
            if (!load_mapdata(self)) {
                sh_info("reload mapdata ok");
            } else {
                sh_error("reload mapdata fail");
            } 
        } else {
            sh_error("reload tplt ok");
        }
    }
}
