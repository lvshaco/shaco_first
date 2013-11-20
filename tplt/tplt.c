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

static struct tplt* self = NULL;

int
tplt_init(const struct tplt_desc* desc, int sz) {
    if (sz <= 0)
        return 1;

    int maxtype = 0;
    const struct tplt_desc* d;
    int i;
    for (i=0; i<sz; ++i) {
        d = &desc[i];
        assert(d->stream);
        if (maxtype < d->type)
            maxtype = d->type;
    } 

    maxtype += 1;
    self = malloc(sizeof(*self));
    self->sz = maxtype;
    self->p = malloc(sizeof(struct tplt_one) * maxtype);
    memset(self->p, 0, sizeof(struct tplt_one) * maxtype);
 
    struct tplt_holder* holder; 
    struct tplt_visitor* visitor;
    for (i=0; i<sz; ++i) {
        d = &desc[i];
        assert(d->stream);
        assert(d->type >= 0 && d->type < maxtype);
        if (d->isfromfile) {
            TPLT_LOGINFO("load tplt: %s", d->stream);
            holder = tplt_holder_load(d->stream, d->size);
        } else {
            holder = tplt_holder_loadfromstream(d->stream, d->streamsz, d->size);
        }
        if (holder == NULL) {
            tplt_fini();
            return 1;
        }
        assert(d->vist);
        visitor = tplt_visitor_create(d->vist, holder);
        if (visitor == NULL) {
            free(holder);
            tplt_fini();
            return 1;
        }
        self->p[d->type].holder = holder;
        self->p[d->type].visitor = visitor;
    }
    return 0;
}

void 
tplt_fini() {
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
tplt_get_holder(int type) {
    if (type >= 0 && type < self->sz)
        return self->p[type].holder;
    return NULL;
}
const struct tplt_visitor* 
tplt_get_visitor(int type) {
    if (type >= 0 && type < self->sz)
        return self->p[type].visitor;
    return NULL;
}
