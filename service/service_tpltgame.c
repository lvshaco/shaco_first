#include "sc_service.h"
#include "sc_util.h"
#include "sc_log.h"
#include "sc_util.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "roommap.h"
#include "map.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

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
_freemapcb(void* value) {
    struct roommap* m = value;
    roommap_free(m);
}

static int
_loadmap(struct tpltgame* self) {
    const struct tplt_holder* holder = tplt_get_holder(TPLT_MAP);
    if (holder == NULL)
        return 1;
    
    int sz = TPLT_HOLDER_NELEM(holder);
    if (self->maps) {
        idmap_free(self->maps, _freemapcb);
        self->maps = NULL;
    } 
    self->maps = idmap_create(sz);
   
    char fname[PATH_MAX];
    const struct map_tplt* tplt = TPLT_HOLDER_FIRSTELEM(map_tplt, holder);
    int i;
    for (i=0; i<sz; ++i) {
        snprintf(fname, sizeof(fname), "./res/map/map%d.map", tplt[i].id);
        sc_info("load map: %s", fname);
        struct roommap* m = roommap_create(fname);
        if (m == NULL) {
            sc_error("load map fail");
            return 1;
        }
        idmap_insert(self->maps, tplt[i].id, m);
    }
    return 0;
}

struct tpltgame*
tpltgame_create() {
    struct tpltgame* self = malloc(sizeof(*self));
    memset(self, 0, sizeof(*self));
    return self;
}

void
tpltgame_free(struct tpltgame* self) {
    tplt_fini();
    if (self->maps) {
        idmap_free(self->maps, _freemapcb);
        self->maps = NULL;
    }
}

int
tpltgame_init(struct service* s) {
    struct tpltgame* self = SERVICE_SELF;
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
    struct tpltgame* self = SERVICE_SELF;
    if (sc_cstr_compare_int32("GMAP", sm->type)) {
        sm->result = idmap_find(self->maps, sm->sessionid);
    } else if (sc_cstr_compare_int32("TPLT", sm->type)) {
        _loadtplt();
        _loadmap(self);
    } 
}
