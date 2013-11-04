#ifndef __freelist_h__
#define __freelist_h__

#define FREELIST(type) \
    int sz; \
    struct type* free; \
    struct type* head; \
    struct type* tail;

#define FREELIST_INIT(fl) do { \
    memset((fl), 0, sizeof(*(fl))); \
} while (0)

#define FREELIST_FINI(type, fl) do { \
    struct type* tmp;           \
    while ((fl)->free) {        \
        tmp = (fl)->free;       \
        (fl)->free = (fl)->free->next; \
        free(tmp);              \
    }                           \
    while ((fl)->head) {        \
        tmp = (fl)->head;       \
        (fl)->head = (fl)->head->next; \
        free(tmp);              \
    }                           \
    (fl)->tail = NULL;          \
    (fl)->sz = 0;               \
} while(0)

#define FREELIST_ALLOC(type, fl, size) ({ \
    struct type* one = (fl)->free;  \
    if (one == NULL) {              \
        one = malloc(size); \
        (fl)->sz++;                 \
    } else {                        \
        (fl)->free = one->next;     \
    }                               \
    one;                            \
})

#define FREELIST_PUSH(type, fl, one) do { \
    one->next = NULL;               \
    if ((fl)->head == NULL) {       \
        (fl)->head = one;           \
        (fl)->tail = one;           \
    } else {                        \
        assert((fl)->tail);         \
        assert((fl)->tail->next == NULL); \
        (fl)->tail->next = one;     \
        (fl)->tail = one;           \
    }                               \
} while(0)

#define FREELIST_POP(type, fl) ({ \
    struct type* pop = (fl)->head; \
    if (pop) {                     \
        (fl)->head = pop->next;    \
        pop->next = (fl)->free;    \
        (fl)->free = pop;          \
    }                              \
    pop;                           \
})

#define FREELIST_POPALL(type, fl) do {  \
    if ((fl)->head) {                   \
        (fl)->tail->next = (fl)->free;  \
        (fl)->free = (fl)->head;        \
        (fl)->head = (fl)->tail = NULL; \
    }                                   \
} while(0)

#endif
