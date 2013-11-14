#include "host.h"
#include "host_env.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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
        switch (lua_type(L, -1)) {
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
            host_setnumenv(key, lua_tonumber(L, -1));
            break;
        case LUA_TSTRING:
            host_setenv(key, lua_tostring(L, -1));
            break;
        default:
            fprintf(stderr, "Invalid config table key %s\n", key);
            exit(1);
        }
		lua_pop(L,1);
	}
	lua_pop(L,1);
}

static void
host_env_load(const char* file) {
    lua_State* L = luaL_newstate();
    if (luaL_dofile(L, file) != LUA_OK) {
        fprintf(stderr, "Error load config file, %s\n", lua_tostring(L, -1));
        exit(1);
    }
    _init_env(L); 
    lua_close(L);
}

int 
main(int argc, char* argv[]) {
    const char* file = "config.lua";
    if (argc > 1) {
        file = argv[1];
    }
    bool isdaemon = false;
    if (argc > 2) {
        if (strcmp(argv[2], "-d") == 0) {
            daemon(1, 1);
            isdaemon = true;
        }
    }
    host_env_init();
    host_env_load(file);
    host_setnumenv("host_daemon", isdaemon);

    if (host_create() == 0) {
        host_start();
        host_free();
    }

    host_env_fini();
    return 0;
}
