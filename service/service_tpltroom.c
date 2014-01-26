#include "sc_service.h"
#include "sc_node.h"
#include "sc_log.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "mapdatamgr.h"
#include <string.h>

static int
load_tplt() {
    tplt_fini();
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ITEM, sizeof(struct item_tplt), 1, TBLFILE(item), 0, TPLT_VIST_VEC32},
        { TPLT_MAP,  sizeof(struct map_tplt),  1, TBLFILE(map),  0, TPLT_VIST_VEC32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
}

static int
load_mapdata() {
    mapdatamgr_fini();
    return mapdatamgr_init("./res/map");
}

void
tpltroom_free(void *null) {
    mapdatamgr_fini();
    tplt_fini();
}

int
tpltroom_init(struct service* s) {
    if (load_tplt()) {
        return 1;
    }
    if (load_mapdata()) {
        return 1; 
    }
    return 0;
}

void
tpltroom_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    if (type != MT_TEXT)
        return;

    if (!strncmp("reload", msg, sz)) {
        if (!load_tplt()) {
            sc_info("reload tplt ok");
            if (!load_mapdata()) {
                sc_info("reload mapdata ok");
            } else {
                sc_error("reload mapdata fail");
            } 
        } else {
            sc_error("reload tplt ok");
        }
    }
}
