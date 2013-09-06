#include "host_service.h"
#include "host_log.h"
#include "array.h"
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>

#define INIT_COUNT 8

struct service_holder {
    struct array* sers;
};

static struct service_holder* S = NULL;

static struct service*
_find(const char* name) {
    int i;
    for (i=0; i<array_size(S->sers); ++i) {
        struct service* s = array_get(S->sers, i);
        if (s && strcmp(s->dl.name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static inline void
_insert(struct service* s) {
    s->serviceid = array_push(S->sers, s);
}
/*
static struct service*
_remove(const char* name) {
    int i;
    for (i=0; i<array_size(S->sers); ++i) {
        struct service* s = array_get(S->sers, i);
        if (s && strcmp(s->dl.name, name) == 0) {
            assert(s->serviceid >= 0);
            array_set(S->sers, s->serviceid, NULL);
            return s;
        }
    }
    return NULL;
}
*/
static int
_create(const char* name) {
    struct service* s = malloc(sizeof(*s));
    if (dlmodule_load(&s->dl, name)) {
        free(s);
        return 1;
    }
    _insert(s);
    return 0;
}

int
service_init(const char* config) {
    S = malloc(sizeof(*S));
    S->sers = array_new(INIT_COUNT);
    return 0;
}

void
service_fini() {
    if (S == NULL) 
        return;
    if (S->sers) {
        int i;
        for (i=0; i<array_size(S->sers); ++i) {
            struct service* s = array_get(S->sers, i);
            if (s) {
                dlmodule_close(&s->dl);
                free(s);
            }
        }
        array_free(S->sers);
    }
    free(S);
}

int 
service_load(const char* name) {
    if (name[0] == '\0') {
        return 1;
    }
    int sz = array_size(S->sers);

    size_t len = strlen(name);
    char tmp[len+1];
    strcpy(tmp, name);

    char* p = tmp;
    char* next = strchr(p, ',');
    while (next) {
        *next = '\0';
        if (_create(p)) {
            return 1;
        } 
        p = next+1; 
        next = strchr(p, ',');
    }
    if (_create(p)) {
        return 1;
    }

    struct service* s;
    int i;
    for (i=sz; i<array_size(S->sers); ++i) {
        s = array_get(S->sers, i);
        if (s && s->dl.init) {
            if (s->dl.init(s)) {
                return 1;
            }
        }
    }
    return 0;
}

int
service_reload(const char* name) {
    struct service* s = _find(name);
    if (s == NULL) {
        return _create(name);
    } else {
        assert(s->dl.handle);
        if (dlmodule_reload(&s->dl)) {
            return 1;
        }
        if (s->dl.reload) {
            s->dl.reload(s);
        }
        return 0;
    }
}

int
service_query_id(const char* name) {
    struct service* s = _find(name);
    return s ? s->serviceid : -1;
}

int 
service_notify_service_message(int destination, struct service_message* sm) {
    struct service* s = array_get(S->sers, destination);
    if (s && s->dl.service) {
        s->dl.service(s, sm);
        return 0;
    }
    return 1;
}

int 
service_notify_net_message(int destination, struct net_message* nm) {
    struct service* s = array_get(S->sers, destination);
    if (s && s->dl.net) {
        s->dl.net(s, nm);
        return 0;
    }
    return 1;
}

int 
service_notify_time_message(int destination) {
    struct service* s = array_get(S->sers, destination);
    if (s && s->dl.time) {
        s->dl.time(s);
        return 0;
    }
    return 1;
}
