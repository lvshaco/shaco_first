/**
 * @file baselib.c
 * @brief   base functions
 * @author lvxiaojun
 * @version 
 * @Copyright shengjoy.com
 * @date 2012-10-12
 */
#define LUA_COMPAT_ALL
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <time.h>

static int base_localtime(lua_State* L) {
    struct tm* ts;
    time_t t; 
    if (lua_gettop(L) == 0) {
        t = time(NULL);
    } else {
        t = luaL_checknumber(L, 1);
    }
    ts = localtime(&t);
    lua_pushnumber(L, ts->tm_year + 1900);
    lua_pushnumber(L, ts->tm_mon + 1);
    lua_pushnumber(L, ts->tm_mday);
    lua_pushnumber(L, ts->tm_hour);
    lua_pushnumber(L, ts->tm_min);
    lua_pushnumber(L, ts->tm_sec);
    return 6;
}

static const struct luaL_Reg baselib[] = {
    {"localtime", base_localtime},
    {NULL, NULL},
};

LUALIB_API int luaopen_liblbase(lua_State* L) {
    luaL_register(L, "", baselib);
    return 1;
}
