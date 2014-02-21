#include "mapdatamgr.h"
#include "roommap.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "sh.h"
#include <limits.h>
#include <stdio.h>

struct sh_hash *
mapdatamgr_init(struct tplt *T, const char *path) {
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
    mapdatamgr_fini(H);
    return NULL;
}

static void
freemapcb(void* m) {
    roommap_free((struct roommap*)m);
}

void
mapdatamgr_fini(struct sh_hash *H) {
    if (H == NULL) {
        return;
    }
    sh_hash_foreach(H, freemapcb);
    sh_hash_delete(H);
}

struct roommap *
mapdatamgr_find(struct sh_hash *H, uint32_t id) {
    if (H)
        return sh_hash_find(H, id);
    return NULL;
}
