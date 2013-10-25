#ifndef __freelist_h__
#define __freelist_h__

#define FREELIST_ENTRY(type, dtype) \
    struct dtype data; \
    struct type* next;

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

#define FREELIST_PUSH(type, fl, value) do { \
    struct type* one = (fl)->free;  \
    if (one == NULL) {              \
        one = malloc(sizeof(*one)); \
        (fl)->sz++;                 \
    } else {                        \
        (fl)->free = one->next;     \
    }                               \
    one->data = *(value);           \
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

#define FREELIST_HEAD(type, fl) \
    (fl)->head ? &(fl)->head->data : NULL

#define FREELIST_POP(type, fl) do { \
    assert((fl)->head);             \
    struct type* pop = (fl)->head;  \
    (fl)->head = pop->next;         \
    pop->next = (fl)->free;         \
    (fl)->free = pop;               \
} while(0)

#define FREELIST_POPALL(type, fl) do {  \
    if ((fl)->head) {                   \
        (fl)->tail->next = (fl)->free;  \
        (fl)->free = (fl)->head;        \
        (fl)->head = (fl)->tail = NULL; \
    }                                   \
} while(0)

#endif
