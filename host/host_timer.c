#include "host_timer.h"
#include "host_service.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INIT_EVENTS 1

struct _event {
    int serviceid;
    int interval;
    uint64_t next_time;
};

struct _event_holder {
    struct _event* p;
    int sz;
    int cap;
};

static void
_event_holder_init(struct _event_holder* eh) {
    eh->cap = INIT_EVENTS;
    eh->sz = 0;
    eh->p = malloc(sizeof(struct _event) * INIT_EVENTS);
}

static void
_event_holder_fini(struct _event_holder* eh) {
    free(eh->p);
    eh->sz = 0;
    eh->cap = 0;
}

static void
_event_holder_grow(struct _event_holder* eh) {
    int old_cap = eh->cap;
    eh->cap *= 2;
    eh->p = realloc(eh->p, sizeof(struct _event) * eh->cap);
    memset(eh->p + old_cap, 0, sizeof(struct _event) * (eh->cap - old_cap));
}

struct host_timer {
    uint64_t start_time;
    uint64_t elapsed_time;
    bool dirty;
    int trigger_time;
    struct _event_holder eh;
};

static struct host_timer* T = NULL;

static uint64_t
_elapsed() {
    if (!T->dirty) {
        return T->elapsed_time;
    }
    T->dirty = false;
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    return ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
}

static uint64_t
_now() {
    struct timespec ti;
    clock_gettime(CLOCK_REALTIME, &ti);
    return ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
}

int 
host_timer_init() {
    T = malloc(sizeof(*T));
    T->dirty = true;
    T->elapsed_time = _elapsed();
    T->start_time = _now() - T->elapsed_time;
    T->trigger_time = -1;
    _event_holder_init(&T->eh);
    return 0;
}

void 
host_timer_fini() {
    if (T == NULL) 
        return;
    _event_holder_fini(&T->eh);
    free(T);
    T = NULL;
}

uint64_t 
host_timer_now() {
    return T->start_time + T->elapsed_time;
}

static uint64_t
_closest_time() {
    struct _event_holder* eh = &T->eh;
    struct _event* e;
    int i;
    uint64_t min = -1;
    for (i=0; i<eh->sz; ++i) {
        e = &eh->p[i];
        if (e->next_time < min) {
            min = e->next_time;
        }
    }
    return min;
}

int
host_timer_max_timeout() {
    T->dirty = true;
    T->elapsed_time = _elapsed();
    uint64_t next_time = _closest_time(T->elapsed_time);
    int timeout = -1;
    if (next_time != -1) {
        timeout = next_time > T->elapsed_time ?
            next_time - T->elapsed_time : 0;
    }
    T->trigger_time = next_time;
    T->dirty = true;
    return timeout;
}

void
host_timer_dispatch_timeout() {
    //T->dirty = true;
    //T->elapsed_time = _elapsed();
    struct _event_holder* eh = &T->eh;
    struct _event* e;
    int i;
    for (i=0; i<eh->sz; ++i) {
        e = &eh->p[i];
        if (e->next_time == T->trigger_time) {
            service_notify_time_message(e->serviceid);
            e->next_time += e->interval;
        }
    }
}

void
host_timer_register(int serviceid, int interval) {
    struct _event_holder* eh = &T->eh;
    if (eh->sz >= eh->cap)  {
       _event_holder_grow(eh);
    }
    struct _event* e = &eh->p[eh->sz];
    e->serviceid = serviceid;
    e->interval = interval;
    e->next_time = T->elapsed_time + interval;
    eh->sz += 1;
}
