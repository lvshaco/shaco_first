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

#define MOD_LEN 32

// dl
static void
_dlclose(struct dlmodule* dl) {
    assert(dl->handle);
    dlclose(dl->handle);
    dl->handle = NULL;
    dl->create = NULL;
    dl->free = NULL;
    dl->init = NULL;
    dl->time = NULL;
    dl->net  = NULL;
    dl->send = NULL;
    dl->main = NULL;
}

static int
_dlopen(struct dlmodule* dl) {
    assert(dl->handle == NULL);

    int len = strlen(dl->name);
    char name[len+7+1];
    int n = snprintf(name, sizeof(name), "mod_%s.so", dl->name);

    char fname[n+2+1];
    snprintf(fname, sizeof(fname), "./%s", name);
    void* handle = dlopen(fname, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        sh_error("Module %s open error: %s", fname, dlerror());
        return 1;
    }
    
    dl->handle = handle;

    char sym[len+10+1];
    strcpy(sym, dl->name);
    strcpy(sym+len, "_create");
    dl->create = dlsym(handle, sym);
    strcpy(sym+len, "_free");
    dl->free = dlsym(handle, sym);
    strcpy(sym+len, "_init");
    dl->init = dlsym(handle, sym);
    strcpy(sym+len, "_time");
    dl->time = dlsym(handle, sym);
    strcpy(sym+len, "_net");
    dl->net = dlsym(handle, sym);
    strcpy(sym+len, "_send");
    dl->send = dlsym(handle, sym);
    strcpy(sym+len, "_main");
    dl->main = dlsym(handle, sym);
 
    if (dl->main == NULL &&
        dl->send == NULL &&
        dl->net  == NULL &&
        dl->time == NULL) {
        sh_error("Module %s no symbol", fname);
        _dlclose(dl);
        return 1;
    }
    if (dl->create && 
        dl->free == NULL) {
        sh_error("Module %s probably memory leak", fname);
        _dlclose(dl);
        return 1;
    }
    return 0;
}

void
_dlunload(struct dlmodule* dl) {
    if (dl == NULL)
        return;
   
    if (dl->free) {
        // sometimes need free, even if no dl->content
        dl->free(dl->content);
        dl->content = NULL;
    }
    if (dl->handle) {
        _dlclose(dl);
    }
    if (dl->name) {
        free(dl->name);
        dl->name = NULL;
    }
}

int
_dlload(struct dlmodule* dl, const char* name) {
    memset(dl, 0, sizeof(*dl));
    int len = strlen(name);
    if (len > MOD_LEN) {
        sh_error("Module `%s` name too long", name);
        return 1;
    }
    dl->name = malloc(len+1);
    strcpy(dl->name, name);
    if (_dlopen(dl)) {
        free(dl->name);
        dl->name = NULL;
        return 1;
    }
    if (dl->create) {
        dl->content = dl->create();
    }
    return 0;
}

#define INIT_COUNT 8

// module
static struct {
    int cap;
    int sz;
    struct module **p;
} *M = NULL;

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
    int len = strlen(name);
    if (len > (2*MOD_LEN + 1)) {
        sh_error("Module `%s` name too long", name);
        return 1;
    }
    char tmp[len+1];
    strcpy(tmp, name);

    struct module *s;
    const char *sname, *fname;
    char *p = strchr(tmp, ':');
    if (p) {
        p[0] = '\0';
        sname = p+1;
    } else {
        sname = name;
    }
    fname = tmp;
    s = _find(sname);
    if (s) {
        sh_error("Module `%s` already exist", sname);
        return 1;
    }
    s = malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    len -= (p-fname) + 1;
    s->name = malloc(len+1);
    strcpy(s->name, sname);
    if (_dlload(&s->dl, fname)) {
        free(s);
        return 1;
    }
    _insert(s); 
    sh_info("Moulde `%s` load ok", name);
    return 0;
}

static int
_init(struct module* s) {
    if (s->dl.init && !s->inited) {
        if (s->dl.init(s)) { 
            return 1;
        }
        s->inited = true;
        sh_info("Module `%s` init ok", s->name);
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
        if (m->dl.handle) {
            if (!strcmp(m->dl.name, s->dl.name)) {
                _dlclose(&m->dl);
                sh_info("Moudle `%s` unload ok", m->name);
            }
        }
    }
    for (i=0; i<M->sz; ++i) {
        m = M->p[i];
        if (m->dl.handle == NULL) {
            if (_dlopen(&m->dl)) {
                sh_error("Module `%s` reload fail", m->name);
            } else {
                sh_info ("Module `%s` reload ok", m->name);
            }
        }
    }
    return 0;
}

int 
module_load(const char* name) {
    assert(name);
    size_t len = strlen(name);
    char tmp[len+1];
    strcpy(tmp, name);
    
    char *save = NULL, *one;
    one = strtok_r(tmp, ",", &save);
    while (one) {
        if (_create(one)) {
            return 1;
        } 
        one = strtok_r(NULL, ",", &save);
    }
    return 0;
}

int 
module_init(const char* name) {
    struct module* s;
    if (name) {
        s = _find(name);
        if (s) {
            return _init(s);
        }
        return 1;
    } else {
        int i;
        for (i=0; i<M->sz; ++i) {
            s = M->p[i];
            if (_init(s)) {
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
    }
}

int 
module_reload_byid(int moduleid) {
    struct module* s = _index(moduleid);
    return _reload(s);
}

int
module_query_id(const char* name) {
    struct module* s = _find(name);
    return s ? s->moduleid : MODULE_INVALID;
}

const char* 
module_query_module_name(int moduleid) {
    struct module* s = _index(moduleid);
    return s->dl.name;
}

int 
module_main(int moduleid, int session, int source, int type, const void *msg, int sz) {
    struct module *s = _index(moduleid);
    if (s->dl.main) {
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
module_mgr_init() {
    M = malloc(sizeof(*M));
    M->cap = INIT_COUNT;
    M->p = malloc(sizeof(M->p[0]) * M->cap);
    M->sz = 0;
    const char* modules = sh_getstr("sh_module", "");
    if (modules[0] &&
        module_load(modules)) {
        sh_exit("module_mgr_init `%s` fail", modules);
    }
}

static void
module_mgr_fini() {
    if (M == NULL) 
        return;
    struct module *s;
    int i;
    for (i=0; i<M->sz; ++i) {
        s = M->p[i];
        _dlunload(&s->dl);
        free(s->name);
        free(s);
    }
    free(M->p);
    M->p = NULL;
    M->cap = 0;
    M->sz = 0;
    free(M);
}

static void
module_mgr_prepare() {
    if (module_init(NULL)) {
        sh_exit("module_mgr_prepare fail");
    }
}

SH_LIBRARY_INIT_PRIO(module_mgr_init, module_mgr_fini, 11)
SH_LIBRARY_INIT_PRIO(module_mgr_prepare, NULL, 50)
