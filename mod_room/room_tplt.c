#include "room_tplt.h"
#include "room_map.h"
#include "room.h"
#include "sh.h"

// mapdata
static void
freemapcb(void* m) {
    roommap_free((struct roommap*)m);
}

void
mapdata_fini(struct sh_hash *H) {
    if (H == NULL) {
        return;
    }
    sh_hash_foreach(H, freemapcb);
    sh_hash_delete(H);
}

struct sh_hash *
mapdata_init(struct tplt *T, const char *path) {
    const struct tplt_holder* holder = tplt_get_holder(T, TPLT_MAP);
    if (holder == NULL) {
        return NULL;
    } 
    struct sh_hash *H = sh_hash_new(1);
    
    char fname[PATH_MAX];
    int i; 
    int sz;
    const struct map_tplt* tplt;
    struct roommap *m;

    tplt = TPLT_HOLDER_FIRSTELEM(map_tplt, holder); 
    sz   = TPLT_HOLDER_NELEM(holder);
    for (i=0; i<sz; ++i) {
        snprintf(fname, sizeof(fname), "%s/map%d.map", path, tplt[i].id);
        sh_info("load map: %s", fname);
        m = roommap_create(fname);
        if (m == NULL) {
            sh_error("load map fail");
            goto err;
        }
        sh_hash_insert(H, tplt[i].id, m);
    }
    return H;
err:
    mapdata_fini(H);
    return NULL;
}
// end mapdata

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
    mapdata_fini(self->MH);
    self->MH = mapdata_init(self->T, "./res/map");
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
        mapdata_fini(self->MH);
        self->MH = NULL;
    }
    if (self->T) {
        tplt_free(self->T);
        self->T = NULL;
    }
}

int
room_tplt_main(struct module *s) {
    struct room *self = MODULE_SELF;
    if (!load_tplt(self)) {
        sh_info("reloadres tplt ok");
        if (!load_mapdata(self)) {
            sh_info("reloadres mapdata ok");
            return 0;
        } else {
            sh_error("reloadres mapdata fail");
            return 1;
        } 
    } else {
        sh_error("reloadres tplt ok");
        return 1;
    }
}
