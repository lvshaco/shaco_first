#include "host_env.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <stdlib.h>

struct host_env {
    lua_State* L;
};

static struct host_env* E = NULL;

void 
host_env_init() {
    E = malloc(sizeof(*E));
    E->L = luaL_newstate(); 
}

void
host_env_fini() {
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
host_getenv(const char* key) {
    lua_State* L = E->L;
    lua_getglobal(L, key);
    const char* str = lua_tostring(L, -1);
    lua_pop(L, 1);
    return str;
}

void 
host_setenv(const char* key, const char* value) {
    lua_State* L = E->L;
    lua_pushstring(L, value);
    lua_setglobal(L, key);
}

float
host_getnum(const char* key, float def) {
    lua_State* L = E->L;
    lua_getglobal(L, key);
    if (lua_isnumber(L, -1)) {
        return lua_tonumber(L, -1);
    } else {
        return def;
    }
}

const char* 
host_getstr(const char* key, const char* def) {
    lua_State* L = E->L;
    lua_getglobal(L, key);
    if (lua_isstring(L, -1)) {
        return lua_tostring(L, -1);
    } else {
        return def;
    }
}

void 
host_setnumenv(const char* key, float value) {
    lua_State* L = E->L;
    lua_pushnumber(L, value);
    lua_setglobal(L, key);
}
