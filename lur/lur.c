#include "lur.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <string.h>
#include <stdlib.h>

struct lur {
    lua_State* L;
};

/*
static void
_dump(lua_State* L) {
    int i;
    for (i=1; i<=lua_gettop(L); ++i) {
        printf("%d: %s\n", i, lua_typename(L, lua_type(L, i)));
    }
}
*/

static int
_getvalue(lua_State* L, const char* key) {
    char* first = strchr(key, '.');
    if (first == NULL) {
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        return 0;
    }
   
    size_t len = strlen(key);
    char tmp[len+1];
    strcpy(tmp, key);

    first = tmp + (ptrdiff_t)(first - key);
    *first = '\0';

    lua_pushstring(L, tmp);
    lua_gettable(L, -2);
    if (!lua_istable(L, -1)) {
        return 1;
    }
    
    char *last = strrchr(first+1, '.');
    if (last) {
        *last = '\0';
        char* node = first;
        while (node) {
            node += 1;
            char* chr = strchr(node, '.');
            if (chr) {
                *chr = '\0';
            }
            lua_pushstring(L, node);
            lua_gettable(L, -2);
            if (!lua_istable(L, -1)) {
                return 1;
            }
            node = chr;
        }
    } else {
        last = first;
    }
    lua_pushstring(L, last + 1);
    lua_gettable(L, -2);
    return 0;
}

int 
lur_getint(struct lur* self, const char* key, int def) {
    return lur_getfloat(self, key, def);
}

float
lur_getfloat(struct lur* self, const char* key, float def) {
    struct lua_State* L = self->L;
    int top = lua_gettop(L);

    float r;
    if (_getvalue(L, key) || !lua_isnumber(L, -1)) {
        r = def;
    } else {
        r = lua_tonumber(L, -1);   
    }
    lua_settop(L, top);
    return r;
}

const char*
lur_getstr(struct lur* self, const char* key, const char* def) {
    struct lua_State* L = self->L;
    int top = lua_gettop(L);
 
    const char* r;
    if (_getvalue(L, key) || !lua_isstring(L, -1)) {
        r = def;
    } else {
        r = lua_tostring(L, -1); 
    }
    lua_settop(L, top);
    return r;
}

int
lur_getnode(struct lur* self, const char* key) {
    struct lua_State* L = self->L;
    int top = lua_gettop(L);

    if (_getvalue(L, key) || !lua_istable(L, -1)) {
        lua_settop(L, top);
        return 0;
    }
    if (lua_gettop(L) - top > 1) {
        lua_replace(L, top+1);
        lua_settop(L, top+1);
    }
    lua_pushnil(L);
    if (lua_next(L, -2) == 0) {
        lua_settop(L, top);
        return 0;
    }
    return 1;
}

int
lur_nextnode(struct lur* self) {
    struct lua_State* L = self->L;
    if (!lua_istable(L, -3)) {
        return 0;
    }
    lua_pop(L, 1);
    if (lua_next(L, -2) == 0) {
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

int
lur_root(struct lur* self, const char* root) {
    struct lua_State* L = self->L;
    lua_getglobal(L, root);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    return 0;
}

void
lur_unroot(struct lur* self) {
    struct lua_State* L = self->L;
    if (lua_istable(L, -1))
        lua_pop(L, 1);
}

const char*
lur_dofile(struct lur* self, const char* file, const char* root) {
    if (file[0] == '\0') {
        return "no file";
    }
    lua_State* L = self->L;
    int r = luaL_dofile(L, file);
    if (r != LUA_OK) {
        const char* r = lua_tostring(L, -1);
        lua_pop(L, 1);
        if (r[0] == '\0')
            return "unknown error";
        else
            return r;
    }
    if (root[0]) {
        if (lur_root(self, root))
            return "no root node";
    }
    return "";
}

struct lur*
lur_create() {
    struct lur* self = malloc(sizeof(*self));
    self->L = luaL_newstate();
    luaL_openlibs(self->L);
    return self;
}

void
lur_free(struct lur* self) {
    if (self->L) {
        lua_close(self->L);
    }
    free(self);
}
