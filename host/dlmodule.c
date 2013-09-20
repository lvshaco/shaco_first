#include "dlmodule.h"
#include "host_log.h"
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

static int
_open(struct dlmodule* dl) {
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "service_%s.so", dl->name);
    if (n >= sizeof(tmp)) {
        host_error("dlmodule %s, name is too long", dl->name);
        return 1;
    }
    void* handle = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        host_error("dlmodule %s open error: %s", dl->name, dlerror());
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
    strcpy(tmp+len, "_service");
    dl->service = dlsym(handle, tmp);
    strcpy(tmp+len, "_time");
    dl->time = dlsym(handle, tmp);
    strcpy(tmp+len, "_net");
    dl->net = dlsym(handle, tmp);
    strcpy(tmp+len, "_nodemsg");
    dl->nodemsg = dlsym(handle, tmp);
    strcpy(tmp+len, "_usermsg");
    dl->usermsg = dlsym(handle, tmp);
 
    if (dl->service == NULL &&
        dl->time == NULL &&
        dl->net == NULL &&
        dl->nodemsg == NULL &&
        dl->usermsg == NULL) {
        host_error("dlmodule %s no symbol", dl->name);
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
    dl->service = NULL;
    dl->time = NULL;
    dl->net = NULL;
    dl->nodemsg = NULL;
    dl->nodemsg = NULL;
}

int
dlmodule_load(struct dlmodule* dl, const char* name) {
    memset(dl, 0, sizeof(*dl));

    dl->name = malloc(strlen(name)+1);
    strcpy(dl->name, name);

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
    if (dl->name) {
        free(dl->name);
        dl->name = NULL;
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
