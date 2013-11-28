#include "sc_service.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

struct tpltgame {
    struct idmap* maps;
};

static int
_loadtplt() {
    tplt_fini();
#define TBLFILE(name) "./res/tbl/"#name".tbl"
    struct tplt_desc desc[] = {
        { TPLT_ITEM, sizeof(struct item_tplt), 1, TBLFILE(item), 0, TPLT_VIST_VEC32},
        { TPLT_MAP,  sizeof(struct map_tplt),  1, TBLFILE(map),  0, TPLT_VIST_VEC32},
    };
    return tplt_init(desc, sizeof(desc)/sizeof(desc[0]));
}

static void
_freemapcb(uint32_t key, void* value, void* ud) {
    struct roommap* m = value;
    roommap_free(m);
}
static int
_freemap(struct idmap* m) {
    if (m == NULL) return;
    idmap_foreach(m, _freemapcb, NULL);
}

static int
_loadmap(struct tpltgame* self) {
    struct tplt_holder* holder = tplt_get_holder(TPLT_MAP);
    int sz = TPLT_HOLDER_NELEM(holder);
    if (self->maps) {
        _freemap(self->maps);
    } else {
        self->maps = idmap_create(sz);
    }
   
    char fname[PATH_MAX];
    struct map_tplt* tplt = TPLT_HOLDER_FIRSTELEM(map_tplt, holder);
    int i;
    for (i=0; i<sz; ++i) {
        snprintf(fname, sizeof(fname), "./res/map/map%d.map", tplt[i].id);
        struct roommap* m = roommap_create(fname);
        if (m == NULL) {
            return 1;
        }
        idmap_insert(self->maps, tplt[i].id, m);
    }
    return 0;
}

void
tpltgame_free(void* pnull) {
    tplt_fini();
    _freemap(self);
}

int
tpltgame_init(struct service* s) {
    if (_loadtplt()) {
        return 1;
    }
    if (_loadmap(self)) {
        return 1;
    }
    return 0;
}

void
tpltgame_service(struct service* s, struct service_message* sm) {
    _loadtplt();
    _loadmap();
}
