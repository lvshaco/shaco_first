#include "sc_service.h"
#include "sc_node.h"
#include "sc.h"
#include "sh_util.h"
#include "sc_init.h"
#include "sc_env.h"
#include "sc_log.h"
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
        if (s && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}
/*
static struct service *
_find_by_module_name(const char *name) {
    int i;
    for (i=0; i<array_size(S->sers); ++i) {
        struct service* s = array_get(S->sers, i);
        if (s && strcmp(s->dl.name, name) == 0) {
            return s;
        }
    }
    return NULL;
}
*/
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
        if (s && strcmp(s->name, name) == 0) {
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
    char tmp[128];
    sc_strncpy(tmp, name, sizeof(tmp));

    struct service *s;
    const char *sname, *dname;
    char *p = strchr(tmp, ':');
    if (p) {
        p[0] = '\0';
        sname = p+1;
    } else {
        sname = name;
    }
    dname = tmp;
    s = _find(sname);
    if (s) {
        return 0;
    }
    s = malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    sc_strncpy(s->name, sname, sizeof(s->name));
    if (dlmodule_load(&s->dl, dname)) {
        free(s);
        return 1;
    }
    _insert(s); 
    sc_info("load service %s ok", name);
    return 0;
}

static int
_prepare(struct service* s) {
    if (s->dl.init && !s->inited) {
        if (s->dl.init(s)) { 
            return 1;
        }
        s->inited = true;
        sc_info("prepare service %s ok", s->name);
    }
    return 0;
}

static int
_reload(struct service* s) {
    //assert(s->dl.handle); donot do this
    if (dlmodule_reload(&s->dl)) {
        return 1;
    }
    sc_info("reload service %s ok", s->name);
    return 0;
}

int 
service_load(const char* name) {
    if (name[0] == '\0') {
        return 1;
    }

    size_t len = strlen(name);
    char tmp[len+1];
    strcpy(tmp, name);
    
    char* saveptr = NULL;
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

bool 
service_isprepared(const char *name) {
    struct service *s = _find(name);
    return s ? s->inited : false;
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
    if (name == NULL || name[0] == '\0')
        return SERVICE_INVALID;
    struct service* s = _find(name);
    return s ? s->serviceid : SERVICE_INVALID;
}
/*
int
service_query_id_by_module_name(const char* name) {
    if (name == NULL || name[0] == '\0')
        return SERVICE_INVALID;
    struct service* s = _find_by_module_name(name);
    return s ? s->serviceid : SERVICE_INVALID;
}
*/
const char* 
service_query_module_name(int serviceid) {
    struct service* s = array_get(S->sers, serviceid);
    if (s) {
        return s->dl.name;
    }
    return "";
}

static inline void
debug_msg(int source, const char *dest, int type, const void *msg, int sz) {
    switch (type) {
    case MT_TEXT: {
        char tmp[sz+1];
        memcpy(tmp, msg, sz);
        tmp[sz] = '\0';
        sc_debug("[%0x - %s] [T] %s", source, dest, tmp);
        break;
        }
    case MT_UM:
        if (sz >= 2) {
        uint16_t msgid = sh_from_littleendian16((uint8_t *)msg);
        sc_debug("[%0x - %s] [U] %u", source, dest, msgid);
        }
        break;
    }
}

int 
service_main(int serviceid, int session, int source, int type, const void *msg, int sz) {
    struct service *s = array_get(S->sers, serviceid);
    if (s && s->dl.main) {
        debug_msg(source, SERVICE_NAME, type, msg, sz);
        s->dl.main(s, session, source, type, msg, sz);
        return 0;
    }
    return 1;
}

int 
service_send(int serviceid, int session, int source, int dest, int type, const void *msg, int sz) {
    struct service *s = array_get(S->sers, serviceid);
    if (s && s->dl.send) {
        s->dl.send(s, session, source, dest, type, msg, sz);
        return 0;
    }
    return 1;
}

int 
service_net(int serviceid, struct net_message* nm) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.net) {
        s->dl.net(s, nm);
        return 0;
    }
    return 1;
}

int 
service_time(int serviceid) {
    struct service* s = array_get(S->sers, serviceid);
    if (s && s->dl.time) {
        s->dl.time(s);
        return 0;
    }
    return 1;
}

static void
service_init() {
    S = malloc(sizeof(*S));
    S->sers = array_new(INIT_COUNT);

    const char* services = sc_getstr("sc_service", "");
    if (services[0] &&
        service_load(services)) {
        sc_exit("service_load fail, services=%s", services);
    }
}

static void
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

static void
service_prepareall() {
    if (service_prepare(NULL)) {
        sc_exit("service_prepareall fail");
    }
}

SC_LIBRARY_INIT_PRIO(service_init, service_fini, 11)
SC_LIBRARY_INIT_PRIO(service_prepareall, NULL, 50)
