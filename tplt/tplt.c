#include "tplt.h"
#include "tplt_internal.h"
#include "tplt_holder.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct tplt {
    int sz;
    struct tplt_holder** p;
};

static struct tplt* T = NULL;

int 
tplt_init(const struct tplt_desc* desc, int sz) {
    if (sz <= 0)
        return 1;

    int maxtype = 0;
    const struct tplt_desc* one;
    int i;
    for (i=0; i<sz; ++i) {
        one = &desc[i];
        assert(one->name);
        if (maxtype < one->type)
            maxtype = one->type;
    } 

    maxtype += 1;
    T = malloc(sizeof(*T));
    T->sz = maxtype;
    T->p = malloc(sizeof(struct tplt_holder*) * maxtype);
    memset(T->p, 0, sizeof(struct tplt_holder*) * maxtype);
 
    struct tplt_holder* holder; 
    for (i=0; i<sz; ++i) {
        one = &desc[i];
        assert(one->name);
        assert(one->type >= 0 && one->type < maxtype);
        TPLT_LOGINFO("load tplt: %s", one->name);
        holder = tplt_holder_load(one->name, one->size);
        if (holder == NULL) {
            return 1;
        }
        T->p[one->type] = holder;
    }
    return 0;
}

void 
tplt_fini() {
    if (T == NULL)
        return;
    
    struct tplt_holder* holder;
    int i;
    for (i=0; i<T->sz; ++i) {
        holder = T->p[i];
        if (holder) {
            tplt_holder_free(holder);
            T->p[i] = NULL;
        }
    }
    free(T);
    T = NULL;
}

struct tplt_holder* 
tplt_get(int type) {
    if (type >= 0 && type < T->sz)
        return T->p[type];
    return NULL;
}
