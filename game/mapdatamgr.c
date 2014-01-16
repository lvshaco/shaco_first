#include "mapdatamgr.h"
#include "roommap.h"
#include "tplt_include.h"
#include "tplt_struct.h"
#include "sh_hash.h"
#include "sc_log.h"
#include <limits.h>
#include <stdio.h>

static struct sh_hash *H = NULL;

int 
mapdatamgr_init(const char *path) {
    const struct tplt_holder* holder = tplt_get_holder(TPLT_MAP);
    if (holder == NULL) {
        return 1;
    } 
    if (H != NULL) {
        return 1;
    }
    H = sh_hash_new(1);
    
    char fname[PATH_MAX];
    int i; 
    int sz;
    const struct map_tplt* tplt;
    struct roommap *m;

    tplt = TPLT_HOLDER_FIRSTELEM(map_tplt, holder); 
    sz   = TPLT_HOLDER_NELEM(holder);
    for (i=0; i<sz; ++i) {
        snprintf(fname, sizeof(fname), "%s/map%d.map", path, tplt[i].id);
        sc_info("load map: %s", fname);
        m = roommap_create(fname);
        if (m == NULL) {
            sc_error("load map fail");
            goto err;
        }
        sh_hash_insert(H, tplt[i].id, m);
    }
    return 0;
err:
    mapdatamgr_fini();
    return 1;
}

static void
freemapcb(void* m) {
    roommap_free((struct roommap*)m);
}

void
mapdatamgr_fini() {
    if (H == NULL) {
        return;
    }
    sh_hash_foreach(H, freemapcb);
    sh_hash_delete(H);
    H = NULL;
}

struct roommap *
mapdatamgr_find(uint32_t id) {
    if (H)
        return sh_hash_find(H, id);
    return NULL;
}
