#include "dlmodule.h"
#include "sc_util.h"
#include "sc_log.h"
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

static int
_open(struct dlmodule* dl) {
    assert(dl->handle == NULL);
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "service_%s.so", dl->name);
    if (n >= sizeof(tmp)) {
        sc_error("dlmodule %s, name is too long", dl->name);
        return 1;
    }
    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname), "./%s", tmp);
    void* handle = dlopen(fname, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        sc_error("dlmodule %s open error: %s", dl->name, dlerror());
        return 1;
    } 
    size_t len = strlen(dl->name);
    strcpy(tmp, dl->name);
    strcpy(tmp+len, "_create");
    dl->create = dlsym(handle, tmp);
    strcpy(tmp+len, "_free");
    dl->free = dlsym(handle, tmp);
    strcpy(tmp+len, "_init");
    dl->init = dlsym(handle, tmp);
    strcpy(tmp+len, "_reload");
    dl->reload = dlsym(handle, tmp);
    strcpy(tmp+len, "_time");
    dl->time = dlsym(handle, tmp);
    strcpy(tmp+len, "_net");
    dl->net = dlsym(handle, tmp);
    strcpy(tmp+len, "_main");
    dl->main = dlsym(handle, tmp);
 
    if (dl->main == NULL &&
        dl->net == NULL &&
        dl->time == NULL) {
        sc_error("dlmodule %s no symbol", dl->name);
        return 1;
    }
    dl->handle = handle;
    if (dl->create &&
        dl->content == NULL) {
        dl->content = dl->create();
    }
    return 0;
}

static void
_dlclose(struct dlmodule* dl) {
    if (dl->handle == NULL)
        return;
    dlclose(dl->handle);
    dl->handle = NULL;
    dl->create = NULL;
    dl->free = NULL;
    dl->init = NULL;
    dl->reload = NULL;
    dl->time = NULL;
    dl->net = NULL;
    dl->main = NULL;
}

int
dlmodule_load(struct dlmodule* dl, const char* name) {
    memset(dl, 0, sizeof(*dl));
    sc_strncpy(dl->name, name, sizeof(dl->name));
    
    if (_open(dl)) {
        dlmodule_close(dl);
        return 1;
    }
    return 0;
}

void
dlmodule_close(struct dlmodule* dl) {
    if (dl == NULL)
        return;

    if (dl->free && dl->content) {
        dl->free(dl->content);
        dl->content = NULL;
    }
    if (dl->handle) {
        _dlclose(dl);
    }
}

int
dlmodule_reload(struct dlmodule* dl) {
    if (dl->name == NULL) {
        return 1;
    }
    if (dl->handle) {
        _dlclose(dl);
    }
    return _open(dl);
}
