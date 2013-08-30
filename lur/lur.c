#include "lur.h"
#include "lua.h"
//#include "lualib.h"
#include "lauxlib.h"
#include <string.h>
#include <stdlib.h>

struct lur {
    lua_State* L;
};

static int
_getvalue(lua_State* L, const char* key) {
    char* first = strchr(key, '.');
    if (first == NULL) {
        lua_getglobal(L, key);
        return 0;
    }
   
    size_t len = strlen(key);
    char tmp[len+1];
    strcpy(tmp, key);

    first = tmp + (ptrdiff_t)(first - key);
    *first = '\0';

    lua_getglobal(L, tmp);
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
    struct lua_State* L = self->L;
    
    int top = lua_gettop(L);

    int r;
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

const char*
lur_dofile(struct lur* self, const char* file) {
    lua_State* L = self->L;
    int r = luaL_dofile(L, file);
    if (r != LUA_OK) {
        const char* r = lua_tostring(L, -1);
        lua_pop(L, 1);
        return r;
    } else {
        return "";
    }
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

