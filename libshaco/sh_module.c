#include "sh_module.h"
#include "sh_node.h"
#include "sh.h"
#include "sh_util.h"
#include "sh_init.h"
#include "sh_env.h"
#include "sh_log.h"
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>

#define INIT_COUNT 8

struct module_holder {
    int cap;
    int sz;
    struct module **p;
};

static struct module_holder* M = NULL;

static struct module *
_index(int idx) {
    assert(idx >= 0 && idx < M->sz);
    return M->p[idx];
}

static struct module *
_find(const char* name) {
    int i;
    for (i=0; i<M->sz; ++i) {
        struct module* s = M->p[i];
        if (s && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static inline void
_insert(struct module* s) {
    if (M->sz == M->cap) {
        M->cap *= 2;
        M->p = realloc(M->p, sizeof(M->p[0]) * M->cap);
    }
    s->moduleid = M->sz;
    M->p[M->sz++] = s;
}

static int
_create(const char* name) {
    char tmp[128];
    sh_strncpy(tmp, name, sizeof(tmp));

    struct module *s;
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
    sh_strncpy(s->name, sname, sizeof(s->name));
    if (dlmodule_load(&s->dl, dname)) {
        free(s);
        return 1;
    }
    _insert(s); 
    sh_info("load module %s ok", name);
    return 0;
}

static int
_prepare(struct module* s) {
    if (s->dl.init && !s->inited) {
        if (s->dl.init(s)) { 
            return 1;
        }
        s->inited = true;
        sh_info("prepare module %s ok", s->name);
    }
    return 0;
}

static int
_reload(struct module* s) {
    //assert(s->dl.handle); donot do this
    // foreach all same so 
    struct module *m;
    int i;
    for (i=0; i<M->sz; ++i) {
        m = M->p[i];
        if (m && !strcmp(m->dl.name, s->dl.name)) {
            if (dlmodule_unload(&m->dl)) {
                sh_error("unload module %s fail", m->name);
            } else {
                sh_info("unload module %s ok", m->name);
            }
        }
    }
    for (i=0; i<M->sz; ++i) {
        m = M->p[i];
        if (m && m->dl.handle == NULL) {
            if (dlmodule_reopen(&m->dl)) {
                sh_error("reopen module %s fail", m->name);
            } else {
                sh_info("reopen module %s ok", m->name);
            }
        }
    }
    return 0;
}

int 
module_load(const char* name) {
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
module_prepare(const char* name) {
    struct module* s;
    if (name) {
        s = _find(name);
        if (s) {
            return _prepare(s);
        }
        return 1;
    } else {
        int i;
        for (i=0; i<M->sz; ++i) {
            s = M->p[i];
            if (_prepare(s)) {
                return 1;
            }
        }
    }
    return 0;
}

int
module_reload(const char* name) {
    struct module* s = _find(name);
    if (s) {
        return _reload(s);
    } else {
        return 1;
        //return _create(name);
    }
}

int 
module_reload_byid(int moduleid) {
    struct module* s = _index(moduleid);
    return _reload(s);
}

int
module_query_id(const char* name) {
    if (name == NULL || name[0] == '\0')
        return MODULE_INVALID;
    struct module* s = _find(name);
    return s ? s->moduleid : MODULE_INVALID;
}

const char* 
module_query_module_name(int moduleid) {
    struct module* s = _index(moduleid);
    return s->dl.name;
}

static inline void
debug_msg(int source, const char *dest, int type, const void *msg, int sz) {
    switch (type) {
    case MT_TEXT: {
        char tmp[sz+1];
        memcpy(tmp, msg, sz);
        tmp[sz] = '\0';
        sh_debug("[%0x - %s] [T] %s", source, dest, tmp);
        break;
        }
    case MT_UM:
        if (sz >= 2) {
        uint16_t msgid = sh_from_littleendian16((uint8_t *)msg);
        sh_debug("[%0x - %s] [U] %u", source, dest, msgid);
        }
        break;
    }
}

int 
module_main(int moduleid, int session, int source, int type, const void *msg, int sz) {
    struct module *s = _index(moduleid);
    if (s->dl.main) {
        debug_msg(source, MODULE_NAME, type, msg, sz);
        s->dl.main(s, session, source, type, msg, sz);
        return 0;
    }
    return 1;
}

int 
module_send(int moduleid, int session, int source, int dest, int type, const void *msg, int sz) {
    struct module *s = _index(moduleid);
    if (s->dl.send) {
        s->dl.send(s, session, source, dest, type, msg, sz);
        return 0;
    }
    return 1;
}

int 
module_net(int moduleid, struct net_message* nm) {
    struct module* s = _index(moduleid);
    if (s->dl.net) {
        s->dl.net(s, nm);
        return 0;
    }
    return 1;
}

int 
module_time(int moduleid) {
    struct module* s = _index(moduleid);
    if (s->dl.time) {
        s->dl.time(s);
        return 0;
    }
    return 1;
}

static void
module_init() {
    M = malloc(sizeof(*M));
    M->cap = INIT_COUNT;
    M->p = malloc(sizeof(M->p[0]) * M->cap);
    M->sz = 0;
    const char* modules = sh_getstr("sh_module", "");
    if (modules[0] &&
        module_load(modules)) {
        sh_exit("module_load fail, modules=%s", modules);
    }
}

static void
module_fini() {
    if (M == NULL) 
        return;
    struct module *s;
    int i;
    for (i=0; i<M->sz; ++i) {
        s = M->p[i];
        dlmodule_close(&s->dl);
        free(s);
    }
    free(M->p);
    M->p = NULL;
    M->cap = 0;
    M->sz = 0;
    free(M);
}

static void
module_prepareall() {
    if (module_prepare(NULL)) {
        sh_exit("module_prepareall fail");
    }
}

SH_LIBRARY_INIT_PRIO(module_init, module_fini, 11)
SH_LIBRARY_INIT_PRIO(module_prepareall, NULL, 50)
