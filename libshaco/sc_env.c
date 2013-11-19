#include "sc_env.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <stdlib.h>

struct sc_env {
    lua_State* L;
};

static struct sc_env* E = NULL;

void 
sc_env_init() {
    E = malloc(sizeof(*E));
    E->L = luaL_newstate(); 
}

void
sc_env_fini() {
    if (E == NULL)
        return;
    if (E->L) {
        lua_close(E->L);
        E->L = NULL;
    }
    free(E);
    E = NULL;
}

const char* 
sc_getenv(const char* key) {
    const char* str;
    lua_State* L = E->L;
    lua_getglobal(L, key);
    str = lua_tostring(L, -1);
    lua_pop(L, 1);
    return str;
}

void 
sc_setenv(const char* key, const char* value) {
    lua_State* L = E->L;
    lua_pushstring(L, value);
    lua_setglobal(L, key);
}

float
sc_getnum(const char* key, float def) {
    float f;
    lua_State* L = E->L;
    lua_getglobal(L, key);
    if (lua_isnumber(L, -1)) {
        f = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return f;
    } else {
        return def;
    }
}

const char* 
sc_getstr(const char* key, const char* def) {
    const char* str;
    lua_State* L = E->L;
    lua_getglobal(L, key);
    if (lua_isstring(L, -1)) {
        str = lua_tostring(L, -1);
        lua_pop(L, 1);
        return str;
    } else {
        return def;
    }
}

void 
sc_setnumenv(const char* key, float value) {
    lua_State* L = E->L;
    lua_pushnumber(L, value);
    lua_setglobal(L, key);
}
