#include "tplt_visitor.h"
#include "tplt_visitor_ops.h"
#include "tplt_holder.h"
#include <stdlib.h>

struct tplt_visitor*
tplt_visitor_create(const struct tplt_visitor_ops* ops, struct tplt_holder* holder) {
    if (ops == NULL)
        return NULL;
    struct tplt_visitor* visitor = malloc(sizeof(*visitor));
    visitor->ops = ops;
    if (ops->create(visitor, holder)) {
        free(visitor);
        return NULL;
    }
    return visitor;
}

void
tplt_visitor_free(struct tplt_visitor* visitor) {
    if (visitor == NULL)
        return;
    if (visitor->ops) {
        visitor->ops->free(visitor);
        visitor->ops = NULL;
    }
    free(visitor);
}

void* 
tplt_visitor_find(const struct tplt_visitor* visitor, uint32_t key) {
    return visitor->ops->find(visitor, key);
}
