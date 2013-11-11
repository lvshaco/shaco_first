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

struct tplt*
tplt_init(const struct tplt_desc* desc, int sz) {
    if (sz <= 0)
        return NULL;

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
    struct tplt* self = malloc(sizeof(*self));
    self->sz = maxtype;
    self->p = malloc(sizeof(struct tplt_one) * maxtype);
    memset(self->p, 0, sizeof(struct tplt_one) * maxtype);
 
    struct tplt_holder* holder; 
    struct tplt_visitor* visitor;
    for (i=0; i<sz; ++i) {
        d = &desc[i];
        assert(d->name);
        assert(d->type >= 0 && d->type < maxtype);
        TPLT_LOGINFO("load tplt: %s", d->name);
        holder = tplt_holder_load(d->name, d->size);
        if (holder == NULL) {
            tplt_fini(self);
            return NULL;
        }
        assert(d->vist);
        visitor = tplt_visitor_create(d->vist, holder);
        if (visitor == NULL) {
            free(holder);
            tplt_fini(self);
            return NULL;
        }
        self->p[d->type].holder = holder;
        self->p[d->type].visitor = visitor;
    }
    return self;
}

void 
tplt_fini(struct tplt* self) {
    if (self == NULL)
        return;
    
    struct tplt_one* one;
    int i;
    for (i=0; i<self->sz; ++i) {
        one = &self->p[i];
        if (one->holder) {
            tplt_holder_free(one->holder);
        }
        if (one->visitor) {
            tplt_visitor_free(one->visitor);
        }
    }
    free(self->p);
    self->p = NULL;
    self->sz = 0;
    free(self);
}

const struct tplt_holder* 
tplt_get_holder(struct tplt* self, int type) {
    if (type >= 0 && type < self->sz)
        return self->p[type].holder;
    return NULL;
}
const struct tplt_visitor* 
tplt_get_visitor(struct tplt* self, int type) {
    if (type >= 0 && type < self->sz)
        return self->p[type].visitor;
    return NULL;
}
