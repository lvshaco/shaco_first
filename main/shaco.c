#include "sh.h"
#include "sh_env.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static void
_init_env(lua_State *L) {
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING) {
			fprintf(stderr, "Invalid config key type\n");
			exit(1);
		}
        const char* key = lua_tostring(L, -2);
        if (key[0] == '_') {
            lua_pop(L,1);
            continue; // no read
        }
        switch (lua_type(L, -1)) {
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
            sh_setnumenv(key, lua_tonumber(L, -1));
            break;
        case LUA_TSTRING:
            sh_setenv(key, lua_tostring(L, -1));
            break;
        default:
            //fprintf(stderr, "Invalid config table key %s\n", key);
            //exit(1);
            break;
        }
		lua_pop(L,1);
	}
	lua_pop(L,1);
}

static void
sh_env_load(const char* file) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    if (luaL_dofile(L, file) != LUA_OK) {
        fprintf(stderr, "Error load config file, %s\n", lua_tostring(L, -1));
        exit(1);
    }
    _init_env(L); 
    lua_close(L);
}

static void
usage(const char* app) {
    fprintf(stderr, "usage: %s config [--key value]\n", app);
}

int 
main(int argc, char* argv[]) {
    const char* file = "config.lua";
    if (argc > 1) {
        file = argv[1];
    }
    
    sh_env_init();
    sh_env_load(file);

    int lastarg;
    int i;
    for (i=2; i<argc; ++i) {
        lastarg = i==argc-1;
        if (!strncmp(argv[i], "--", 2) && 
             argv[i][2] != '\0' &&
            !lastarg) {
            sh_setenv(&(argv[i][2]), argv[i+1]);
            i++;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (sh_getint("sh_daemon", 0)) {
        daemon(1, 1);
    }
    
    sh_init();
    sh_start();
    sh_env_fini();
    return 0;
}
