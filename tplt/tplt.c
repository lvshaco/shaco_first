#include "tplt.h"
#include "tplt_internal.h"
#include "tplt_holder.h"
#include "tplt_visitor.h"
#include "tplt_visitor_ops_implement.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct tplt_one {
    struct tplt_holder* holder;
    struct tplt_visitor* visitor;
};

struct tplt {
    int sz;
    struct tplt_one* p;
};

static struct tplt* T = NULL;

int 
tplt_init(const struct tplt_desc* desc, int sz) {
    if (sz <= 0)
        return 1;

    int maxtype = 0;
    const struct tplt_desc* d;
    int i;
    for (i=0; i<sz; ++i) {
        d = &desc[i];
        assert(d->name);
        if (maxtype < d->type)
            maxtype = d->type;
    } 

    maxtype += 1;
    T = malloc(sizeof(*T));
    T->sz = maxtype;
    T->p = malloc(sizeof(struct tplt_one) * maxtype);
    memset(T->p, 0, sizeof(struct tplt_one) * maxtype);
 
    struct tplt_holder* holder; 
    struct tplt_visitor* visitor;
    for (i=0; i<sz; ++i) {
        d = &desc[i];
        assert(d->name);
        assert(d->type >= 0 && d->type < maxtype);
        TPLT_LOGINFO("load tplt: %s", d->name);
        holder = tplt_holder_load(d->name, d->size);
        if (holder == NULL) {
            return 1;
        }
        assert(d->vist);
        visitor = tplt_visitor_create(d->vist, holder);
        if (visitor == NULL) {
            return 1;
        }
        T->p[d->type].holder = holder;
        T->p[d->type].visitor = visitor;
    }
    return 0;
}

void 
tplt_fini() {
    if (T == NULL)
        return;
    
    struct tplt_one* one;
    int i;
    for (i=0; i<T->sz; ++i) {
        one = &T->p[i];
        if (one->holder) {
            tplt_holder_free(one->holder);
        }
        if (one->visitor) {
            tplt_visitor_free(one->visitor);
        }
    }
    free(T->p);
    T->p = NULL;
    T->sz = 0;
    free(T);
    T = NULL;
}

const struct tplt_holder* 
tplt_get_holder(int type) {
    if (type >= 0 && type < T->sz)
        return T->p[type].holder;
    return NULL;
}
const struct tplt_visitor* 
tplt_get_visitor(int type) {
    if (type >= 0 && type < T->sz)
        return T->p[type].visitor;
    return NULL;
}
