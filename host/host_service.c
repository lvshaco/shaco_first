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
    struct service* s = _find(name);
    if (s) {
        return 0;
    }
    s = malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    if (dlmodule_load(&s->dl, name)) {
        free(s);
        return 1;
    }
    _insert(s); 
    host_info("load service %s ok", name);
    return 0;
}

static int
_prepare(struct service* s) {
    if (s->dl.init && !s->inited) {
        if (s->dl.init(s)) { 
            return 1;
        }
        s->inited = true;
        host_info("prepare service %s ok", s->dl.name);
    }
    return 0;
}

static int
_reload(struct service* s) {
    assert(s->dl.handle);
    if (dlmodule_reload(&s->dl)) {
        return 1;
    }
    if (s->dl.reload) {
        s->dl.reload(s);
    }
    host_info("reload service %s ok", s->dl.name);
    return 0;
}

int
service_init() {
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

    size_t len = strlen(name);
    char tmp[len+1];
    strcpy(tmp, name);
    
    char* saveptr;
    char* one = strtok_r(tmp, ",", &saveptr);
    while (one) {
        if (_create(one)) {
            return 1;
        } 
        one = strtok_r(NULL, ",", &saveptr);
    }
    return 0;
}

int 
service_prepare(const char* name) {
    struct service* s;
    if (name) {
        s = _find(name);
        if (s) {
            return _prepare(s);
        }
        return 1;
        
    } else {
        int i;
        for (i=0; i<array_size(S->sers); ++i) {
            s = array_get(S->sers, i); 
            if (s) {
                if (_prepare(s)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int
service_reload(const char* name) {
    struct service* s = _find(name);
    if (s) {
        return _reload(s);
    } else {
        return 1;
        //return _create(name);
    }
}

int 
service_reload_byid(int serviceid) {
    struct service* s = array_get(S->sers, serviceid);
    if (s) {
        return _reload(s);
    }
    return 1;
}

int
service_query_id(const char* name) {
    struct service* s = _find(name);
    return s ? s->serviceid : SERVICE_INVALID;
}

const char* 
service_query_name(int serviceid) {
    struct service* s = array_get(S->sers, serviceid);
    if (s) {
        return s->dl.name;
    }
    return "";
}

int 
service_notify_service(int serviceid, struct service_message* sm) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.service) {
        s->dl.service(s, sm);
        return 0;
    }
    return 1;
}

int 
service_notify_net(int serviceid, struct net_message* nm) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.net) {
        s->dl.net(s, nm);
        return 0;
    }
    return 1;
}

int 
service_notify_time(int serviceid) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.time) {
        s->dl.time(s);
        return 0;
    }
    return 1;
}

int 
service_notify_nodemsg(int serviceid, int id, void* msg, int sz) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.nodemsg) {
        s->dl.nodemsg(s, id, msg, sz);
        return 0;
    }
    return 1;
}

int 
service_notify_usermsg(int serviceid, int id, void* msg, int sz) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.usermsg) {
        s->dl.usermsg(s, id, msg, sz);
        return 0;
    }
    return 1;
}
