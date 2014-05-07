#include "coroutine.h"
#include <ucontext.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define S_SUSPENDED 0
#define S_RUNNING 1
#define S_DEAD 2

struct coroutine {
    ucontext_t uc;
    int status;
    int id;
    void (*startfn)(void *);
    void *udata;
    struct coroutine *parent;
};

static int co_genid;
static struct coroutine *co_running;
static struct coroutine *co_main;

static inline void
_swap(struct coroutine *from, struct coroutine *to) {
    //printf("+++swap %d -> %d\n", from->id, to->id);
    if (swapcontext(&from->uc, &to->uc)) {
        assert(0 && "swapcontext failure");
    }
}

static inline void
_yield() {
    struct coroutine *co = co_running;
    struct coroutine *to = co->parent;
    co_running->status = S_SUSPENDED;
    co->parent->status = S_RUNNING;
    co_running = to;
    _swap(co, to);
}

static void
_start(uint32_t p1, uint32_t p2) {
    //uint64_t t = p2 << 16;
    //t <<= 16;
    uint64_t t = p2;
    t <<= 32;
    t |= p1;
    struct coroutine *me = (void*)t;
    assert(me == co_running);
    
    me->startfn(me->udata);
    
    assert(me == co_running);
    struct coroutine *to = me->parent;
    to->udata = NULL;
    me->status = S_DEAD;
    to->status = S_RUNNING;
    co_running = to;
    _swap(me, to);
}

struct coroutine *
_create(void (*fn)(void *), int stack_size) {
    struct coroutine *co = malloc(sizeof(*co) + stack_size);
    co->status = S_SUSPENDED;
    co->id = ++co_genid;
    co->startfn = fn;
    co->udata = NULL;
    co->parent = co_main;
    if (getcontext(&co->uc)) {
        free(co);
        return NULL;
    }
    co->uc.uc_stack.ss_sp = co+1;
    co->uc.uc_stack.ss_size = stack_size;
    uint64_t t = (intptr_t)co;
    uint32_t p1, p2;
    p1 = t;
    //t >>= 16;
    //p2 = t >> 16;
    p2 = t >> 32;
    //printf("++++create %d\n", co->id);
    makecontext(&co->uc, (void(*)())_start, 2, p1, p2);
    //printf("++++create %d\n", co->id);
    return co;
}

struct coroutine *
coroutine_create(void (*fn)(void *), int stack_size) {
    if (co_main == NULL) {
        co_main = _create(NULL, 8);
        if (co_main == NULL) {
            return NULL;
        }
        co_main->parent = co_main;
        co_running = co_main;
    }
    return _create(fn, stack_size);
}

void *
coroutine_yield(void *udata) {
    assert(co_running);
    assert(co_running->parent);
    struct coroutine *me = co_running;
    struct coroutine *to = co_running->parent;
    to->udata = udata;
    //printf("+++yield %d -> %d\n", me->id, to->id);
    me->status = S_SUSPENDED;
    to->status = S_RUNNING;
    co_running = to;
    _swap(me, to);

    return me->udata;
}

void *
coroutine_resume(struct coroutine *co, void *udata) {
    //assert(co->status != S_DEAD);
    if (co->status != S_SUSPENDED ||
        co->parent != co_main) {
        return NULL;
    }
    struct coroutine *me = co_running;
    co->udata = udata;
    co->parent = co_running;
    //printf("+++resume %d -> %d\n", me->id, co->id);

    me->status = S_SUSPENDED;
    co->status = S_RUNNING;
    co_running = co;
    _swap(me, co);
    co->parent = co_main;
    return me->udata;
}

const char *
coroutine_status(struct coroutine *co) {
    static const char *S[] = {
        "suspended", "running", "dead",
    };
    assert(co->status >= 0 && co->status < sizeof(S)/sizeof(S[0]));
    return S[co->status];
}

struct coroutine *
coroutine_running() {
    return co_running;
}
