/**
 * @file syslib.c
 * @brief   sys funtions
 * @author lvxiaojun
 * @version 
 * @Copyright shengjoy.com
 * @date 2012-10-12
 */
#define LUA_COMPAT_ALL
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <stdlib.h>
#include <alloca.h>
#include <iconv.h>
#include <string.h>

#ifdef _WIN32
#include <winbase.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

static int sys_getenv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushstring(L, getenv(name));
    return 1;
}

static int sys_setenv(lua_State* L) {
    const char *name = luaL_checkstring(L, 1); 
    const char *value = lua_tostring(L, 2); 

#ifndef _WIN32
    if (!(value ? setenv(name, value, 1)
#if defined(__linux__)
     : unsetenv(name)))
#else
     : ((void) unsetenv(name), 0)))
#endif

#else
    if (SetEnvironmentVariableA(name, value))
#endif
        lua_pushboolean(L, 1);
    else
        lua_pushboolean(L, 0);
    return 1;

}

static int sys_daemon(lua_State* L) {
#ifndef _WIN32
    int nochdir = luaL_checknumber(L, 1);
    int noclose = luaL_checknumber(L, 2);
    daemon(nochdir, noclose);
#endif
    return 1;
}

static int sys_iconv(lua_State* L) {
    size_t len;
    const char* in = luaL_checklstring(L, 1, &len);
    if (len == 0) {
        lua_pushstring(L, "");
        return 1;
    }

    const char* to = luaL_checklstring(L, 2, &len);
    const char* from = luaL_checklstring(L, 3, &len);
    
    iconv_t h = iconv_open(to, from);
    if (h == (iconv_t)-1) {
        return 0;
    }

    size_t insize = strlen(in) + 1;
    size_t outsize = 0;
    if (lua_gettop(L) == 0) {
       outsize = luaL_checknumber(L, 4);
    }
    if (outsize == 0) {
        outsize = insize * 4;
    }

    char* out = (char*)alloca(outsize);
    if (out == NULL) {
        return 0;
    }
    char* sin  = (char*)in;
    char* sout = (char*)out;
    iconv(h, &sin, &insize, &sout, &outsize);
    iconv_close(h);
    lua_pushstring(L, out); 
    return 1;
}

static const struct luaL_Reg syslib[] = {
    {"getenv", sys_getenv},
    {"setenv", sys_setenv},
    {"daemon", sys_daemon},
    {"iconv",  sys_iconv},
    {NULL, NULL},
};

LUALIB_API int luaopen_liblsys(lua_State* L) {
    luaL_register(L, "", syslib);
    return 1;
}

