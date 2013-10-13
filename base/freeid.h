#ifndef __freeid_h__
#define __freeid_h__

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

struct freeid {
    int cap;
    int hashcap; 
    int* ids;
    int* free;
    int* slots;
};

static void 
freeid_init(struct freeid* fi, int cap, int hashcap) {
    int* p;
    int i;
    if (cap <= 0)
        cap = 1;
    if (hashcap < cap)
        hashcap = cap;
    
    p = malloc(sizeof(int) * cap);
    for (i=0; i<cap-1; ++i) {
        p[i] = i+1;
    }
    p[i] = -1;
    fi->ids = p;
    fi->free = &fi->ids[0];
    fi->slots = malloc(sizeof(int) * hashcap);
    for (i=0; i<hashcap; ++i) {
        fi->slots[i] = -1;
    }
    fi->cap = cap;
    fi->hashcap = hashcap;
}

static void
freeid_fini(struct freeid* fi) {
    if (fi == NULL)
        return;

    fi->cap = 0;
    fi->hashcap = 0;
    fi->free = NULL;
    free(fi->ids);
    fi->ids = NULL;
    free(fi->slots);
    fi->slots = NULL;
}

static inline bool
freeid_full(struct freeid* fi, int hash) {
    if (hash >= 0 && hash < fi->hashcap) {
        if (fi->free != NULL)
            return false;
    }
    return true;
}

static inline int
freeid_find(struct freeid* fi, int hash) {
    if (hash >= 0 && hash < fi->hashcap) {
        return fi->slots[hash];
    }
    return -1;
}

static inline int
freeid_alloc(struct freeid* fi, int hash) {
    if (freeid_full(fi, hash))
        return -1;
    int id = fi->free - fi->ids;
    assert(id >= 0);
    int next = *fi->free;
    if (next == -1) {
        fi->free = NULL;
    } else {
        fi->free = &fi->ids[next];
    }
    fi->slots[hash] = id;
    return id;
}

static inline int
freeid_free(struct freeid* fi, int hash) {
    if (hash < 0 || hash >= fi->hashcap) {
        return -1;
    }
    int id = fi->slots[hash];
    if (id == -1) {
        return -1;
    }
    fi->slots[hash] = -1;
    int* free = &fi->ids[id];
    *free = fi->free - fi->ids;
    fi->free = free;
    return id;
}

#endif
