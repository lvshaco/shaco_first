#include "sc_service.h"
#include "sc_node.h"
#include "sc_log.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <string.h>

static int
load_tplt() {
    tplt_fini();
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ROLE, sizeof(struct role_tplt), 1, TBLFILE(role), 0, TPLT_VIST_VEC32},
        { TPLT_RING, sizeof(struct ring_tplt), 1, TBLFILE(ring), 0, TPLT_VIST_VEC32},
        { TPLT_EXP,  sizeof(struct exp_tplt),  1, TBLFILE(exp),  0, TPLT_VIST_INDEX32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
}

void
tplthall_free(void* null) {
    tplt_fini();
}

int
tplthall_init(struct service* s) {
    if (load_tplt()) {
        return 1;
    }
    return 0;
}

void
tplthall_main(struct service *s, int session, int source, int type, const void *msg, int sz) {
    if (type != MT_TEXT)
        return;

    if (strncmp("reload", msg, sz)) {
        if (!load_tplt()) {
            sc_info("reload tplt ok");
        } else {
            sc_error("reload tplt fail");
        }
    }
}
