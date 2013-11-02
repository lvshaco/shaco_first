#include "mpool.h"
#include <stdlib.h>
#include <stdio.h>

struct _page {
    struct _page* next;
    char begin[0];
};

struct _huge_page {
    struct _huge_page* next;
    size_t size;
    char begin[0];
};

struct mpool {
    struct _page* pages;
    size_t npage;
    size_t size;
    size_t used; 
    size_t nalloc;
    struct _huge_page* huges;
    size_t nhuge;
};

struct mpool*
mpool_new(size_t page_size) {
    size_t cap = 1024;
    while (cap < page_size)
        cap *= 2;

    struct mpool* m = malloc(sizeof(*m));
    struct _page* p = malloc(sizeof(*p) + cap);
    p->next = NULL;
    m->pages = p;
    m->npage = 1;
    m->size = cap;
    m->used = 0; 
    m->nalloc = 0;
    m->nhuge = 0;
    return m;
}

void
mpool_delete(struct mpool* m) {
    if (m) {
        struct _page* p; 
        while (m->pages) {
            p = m->pages;
            m->pages = p->next;
            free(p);
        }
        struct _huge_page* hp;
        while (m->huges) {
            hp = m->huges;
            m->huges = hp->next;
            free(hp);
        }
        free(m);
    }
}

void*
mpool_alloc(struct mpool* m, size_t n) {
    n = (n+3) & ~3;
    if (n >= m->size) {
        struct _huge_page* p = malloc(sizeof(*p) + n);
        p->size = n;
        p->next = m->huges;
        m->huges = p;
        m->nhuge += 1;
        m->nalloc += n;
        return p->begin;
    }
    if (m->used + n > m->size) {
        struct _page* p = malloc(sizeof(*p) + m->size); 
        p->next = m->pages;
        m->pages = p;
        m->npage += 1;
        m->used = 0;
        m->nalloc += n;
        return p->begin;
    } else {
        void* ptr = m->pages->begin + m->used;
        m->used += n;
        m->nalloc += n;
        return ptr;
    }
}

void*
mpool_realloc(struct mpool* m, void* p, size_t n) {
    return mpool_alloc(m, n);
}

void 
mpool_dump(struct mpool* m) {
    printf("[npage:%zu] ", m->npage);
    printf("[total pagesize:%zu] ", m->npage*m->size);
     
    printf("[nhuge:%zu] ", m->nhuge);
    size_t n = 0;
    struct _huge_page* h = m->huges;
    while (h) {
        n += h->size;
        h = h->next;
    }
    printf("[total huge size:%zu] ", n);

    printf("[nalloc:%zu]\n", m->nalloc);
}
