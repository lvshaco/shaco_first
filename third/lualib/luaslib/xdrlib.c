/**
 * @file xdrlib.c
 * @brief   xdr message
 * @author lvxiaojun
 * @version 
 * @Copyright shengjoy.com
 * @date 2012-10-04
 */
#include "xdr.h"
#define LUA_COMPAT_ALL
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define MAX_TABLE_LEVEL 4

typedef struct xdr* xdr_ud;

static xdr_ud* _create(lua_State* L, size_t init_size, size_t max_size) {
    xdr_ud* xu = (xdr_ud*)lua_newuserdata(L, sizeof(xdr_ud));
    *xu = xdr_create(init_size, max_size);
    luaL_getmetatable(L, XDR_MT);
    lua_setmetatable(L, -2);
    return xu;
}

static int create(lua_State* L) {
    size_t init_size = 0;
    size_t max_size = 0;
    switch(lua_gettop(L)) {
    case 1:
        init_size = luaL_checknumber(L, 1);
        break;
    case 2:
        init_size = luaL_checknumber(L, 1);
        max_size = luaL_checknumber(L, 2);
        break;
    default:
        break;
    }
    _create(L, init_size, max_size);
    return 1;
}

static xdr_ud* _get_xdr_ud(lua_State*L, int ud) {
    return  (xdr_ud*)luaL_checkudata(L, 1, XDR_MT);
}

static int destroy(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    if (*xu) {
        xdr_destroy(*xu);
        *xu = NULL;
    }
    return 0;
}

static int reset(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    xdr_reset(*xu);
    return 0;
}

static int unpacker(lua_State* L) {
    xdr_ud* xu;
    size_t len;
    const char* value = luaL_checklstring(L, 1, &len);
    size_t size = luaL_checknumber(L, 2);
    if (len > size) len = size;
    xu = _create(L, len, len);
    xdr_setbuffer(*xu, value, len);
    return 1;
}

#define op_check(op) if (op != 0) luaL_error(L, "%s error", __FUNCTION__);

#define declare_pack_number(type, sign) \
    static int _pack_##sign(lua_State* L, int idx, xdr_ud* xu) { \
        type value = (type)luaL_checknumber(L, idx); \
        op_check(xdr_pack##sign(*xu, value)); \
        return 0; \
    } \
    static int pack_##sign(lua_State* L) { \
        xdr_ud* xu = _get_xdr_ud(L, 1); \
        return _pack_##sign(L, 2, xu); \
    }

#define declare_unpack_number(type, sign) \
    static int _unpack_##sign(lua_State*L, xdr_ud* xu) { \
        type value; \
        op_check(xdr_unpack##sign(*xu, &value)); \
        lua_pushnumber(L, value); \
        return 1; \
    } \
    static int unpack_##sign(lua_State* L) { \
        xdr_ud* xu = _get_xdr_ud(L, 1); \
        return _unpack_##sign(L, xu); \
    }

declare_pack_number(int8_t, int8)
declare_pack_number(uint8_t, uint8)
declare_pack_number(int16_t, int16)
declare_pack_number(uint16_t, uint16)
declare_pack_number(int32_t, int32)
declare_pack_number(uint32_t, uint32)
declare_pack_number(float, float)

static int _pack_string(lua_State* L, int idx, xdr_ud* xu) {
    size_t len;
    const char* value = luaL_checklstring(L, idx, &len);
    op_check(xdr_packstring(*xu, value, len));
    return 0;
}
static int pack_string(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    return _pack_string(L, 2, xu);
}


declare_unpack_number(int8_t, int8)
declare_unpack_number(uint8_t, uint8)
declare_unpack_number(int16_t, int16)
declare_unpack_number(uint16_t, uint16)
declare_unpack_number(int32_t, int32)
declare_unpack_number(uint32_t, uint32)
declare_unpack_number(float, float)

static int _unpack_string(lua_State* L, xdr_ud* xu) {
    const char* value;
    uint32_t len;
    op_check(xdr_unpackstring(*xu, &value, &len));
    lua_pushlstring(L, value, len);
    return 1;
}
static int unpack_string(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    return _unpack_string(L, xu);
}

static void _check_key_type(lua_State*L, int key) {
    int type = lua_type(L, key);
    if ((type != LUA_TNUMBER) && (type != LUA_TSTRING))
        luaL_error(L, "error : key type must be number or string");
}

static int _pack_table(lua_State* L, int proto, int inst, xdr_ud* xu, int level);

static int _pack_field(lua_State* L, int proto, int inst, 
        const char* type, int field, xdr_ud* xu, int level) {
    int top;
    lua_rawgeti(L, proto, field); // field name
    _check_key_type(L, -1);
    lua_rawget(L, inst);        // field value
    switch (*type) {
    case 'i':
        _pack_int32(L, -1, xu);
        break;
    case 'u':
        _pack_uint32(L, -1, xu);
        break;
    case 'f':
        _pack_float(L, -1, xu);
        break;
    case 's':
        _pack_string(L, -1, xu);
        break;
    case 't':
        lua_rawgeti(L, proto, field); // field name
        lua_rawget(L, proto);   // sub proto
        top = lua_gettop(L);
        if (_pack_table(L, top, top-1, xu, level+1) != 0)
            return -1;
        lua_pop(L, 1); // pop sub proto
        break;
    default:
        luaL_error(L, "pack table in level#%d error : invalid field type#%s", level, type);
    }
    lua_pop(L, 1);
    return 0; // field value in top
}

static int _pack_fieldlist(lua_State* L, int proto, int inst, 
        const char* type, int field, xdr_ud* xu, int level) {
    int top;
    int len;
    int i;
    len = lua_objlen(L, inst);
    op_check(xdr_packuint32(*xu, len)); 
    for (i=1; i<=len; ++i) {
        lua_rawgeti(L, inst, i); // get field value
        switch (*type) {
        case 'i':
            _pack_int32(L, -1, xu);
            break;
        case 'u':
            _pack_uint32(L, -1, xu);
            break;
        case 'f':
            _pack_float(L, -1, xu);
            break;
        case 's':
            _pack_string(L, -1, xu);
            break;
        case 't':
            lua_rawgeti(L, proto, field); // sub proto
            top = lua_gettop(L);
            if (_pack_table(L, top, top-1, xu, level+1) != 0)
                return -1;
            lua_pop(L, 1); // pop sub proto
            break;
        default:
            luaL_error(L, "pack table in level#%d error : invalid field type#%s", level, type);
        }
    }
    lua_pop(L, len);
    return 0; // field value in top
}

static int _pack_table(lua_State* L, int proto, int inst, xdr_ud* xu, int level) {
    size_t n;
    size_t i;
    size_t len;
    const char* type;
    luaL_checktype(L, proto, LUA_TTABLE);
    luaL_checktype(L, inst, LUA_TTABLE);
    if (level >= MAX_TABLE_LEVEL)
        luaL_error(L, "pack table error : too much level (at most %d)", MAX_TABLE_LEVEL);
    n = lua_objlen(L, proto);
    if (n == 0 || (n&1) != 0)
        luaL_error(L, "pack table in level#%d error : proto table n#%u", level, n);
    for (i=1; i<=n; i+=2) {
        lua_rawgeti(L, proto, i);   // type desc
        type = luaL_checklstring(L, -1, &len);
        if (len == 2 && *(type+1) == 'l') {
            if (_pack_fieldlist(L, proto, inst, type, i+1, xu, level) != 0)
                return -1;
        } else {
            if (_pack_field(L, proto, inst, type, i+1, xu, level) != 0)
                return -1;
        }
        lua_pop(L, 1); // pop type desc, filed value
    }
    return 0;
}

static int pack_table(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    _pack_table(L, 2, 3, xu, 0);
    return 0;
}

static int _unpack_table(lua_State*L, int proto, int inst, xdr_ud* xu, int level);

static int _unpack_field(lua_State*L, int proto, int inst, 
        const char* type, int field, xdr_ud* xu, int level) {
    int top; 
    lua_rawgeti(L, proto, field); // field name
    _check_key_type(L, -1);
    switch (*type) {
    case 'i':
        _unpack_int32(L, xu);
        break;
    case 'u':
        _unpack_uint32(L, xu);
        break;
    case 'f':
        _unpack_float(L, xu);
        break;
    case 's':
        _unpack_string(L, xu);
        break;
    case 't':
        lua_pushvalue(L, -1);
        lua_rawget(L, proto); // get sub proto
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_newtable(L); // new sub inst
        top = lua_gettop(L);
        if (_unpack_table(L, top-1, top, xu, level+1) == -1) 
            return -1;
        lua_replace(L, -2);
        break;
    default:
        luaL_error(L, "unpack table in level#%d error : invalid field type#%s", level, type);
    }

    lua_rawset(L, inst);
    return 0;
}

static int _unpack_fieldlist(lua_State*L, int proto, int inst, 
        const char* type, int field, xdr_ud* xu, int level) {
    uint32_t len;
    uint32_t i;
    int top;
    op_check(xdr_unpackuint32(*xu, &len));
    for (i=1; i<=len; ++i) {
        switch (*type) {
        case 'i':
            _unpack_int32(L, xu);
            break;
        case 'u':
            _unpack_uint32(L, xu);
            break;
        case 'f':
            _unpack_float(L, xu);
            break;
        case 's':
            _unpack_string(L, xu);
            break;
        case 't':
            lua_rawgeti(L, proto, field); // get sub proto
            luaL_checktype(L, -1, LUA_TTABLE);
            lua_newtable(L); // new sub inst
            top = lua_gettop(L);
            if (_unpack_table(L, top-1, top, xu, level+1) == -1) 
                return -1;
            lua_replace(L, -2);
            break;
        default:
            luaL_error(L, "unpack table in level#%d error : invalid field type#%s", level, type);
        }
        lua_rawseti(L, inst, i);
    }
    return 0;
}

static int _unpack_table(lua_State*L, int proto, int inst, xdr_ud* xu, int level) {
    const char* type;
    size_t i;
    size_t n;
    size_t len;
    luaL_checktype(L, proto, LUA_TTABLE);
    luaL_checktype(L, inst, LUA_TTABLE);
    if (level >= MAX_TABLE_LEVEL)
        luaL_error(L, "unpack table error : too much level (at most %d)", MAX_TABLE_LEVEL);
    n = lua_objlen(L, proto);
    if (n == 0 || (n & 1) != 0)
        luaL_error(L, "unpack table in level#%d error : proto table n#%u", level, n);
    for (i=1; i<=n; i+=2) {
        lua_rawgeti(L, proto, i);   // type desc
        type = luaL_checklstring(L, -1, &len);
        if (len == 2 && *(type+1) == 'l') {
            if (_unpack_fieldlist(L, proto, inst, type, i+1, xu, level) != 0)
                return -1;
        } else {
            if (_unpack_field(L, proto, inst, type, i+1, xu, level) != 0)
                return -1;
        }
        lua_pop(L, 1); // pop type desc
    }
    return 0;
}

static int unpack_table(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    lua_settop(L, 2);
    lua_newtable(L);
    _unpack_table(L, 2, 3, xu, 0);
    return 1;
}

static int get_remain(lua_State* L) {
    xdr_ud* xu = _get_xdr_ud(L, 1);
    uint32_t remain = xdr_getremain(*xu);
    lua_pushnumber(L, remain);
    return 1;
}

static int get_buffer(lua_State* L) {
    size_t len;
    xdr_ud* xu = _get_xdr_ud(L, 1);
    const char* buf = xdr_getbuffer(*xu, &len);
    lua_pushlightuserdata(L, (void*)buf);
    lua_pushnumber(L, len);
    return 2;
}

static int set_buffer(lua_State* L) {
    const char* buf;
    uint32_t len;
    xdr_ud* xu = _get_xdr_ud(L, 1);
    if (!lua_islightuserdata(L, 2))
        luaL_error(L, "xdr set_buffer need lightuserdata (const char*)");
    buf = lua_touserdata(L, 2);
    len = luaL_checknumber(L, 3);
    xdr_setbuffer(*xu, buf, len);
    return 0;
}

static int copyout_buffer(lua_State* L) {
    size_t len;
    xdr_ud* xu = _get_xdr_ud(L, 1);
    const char* buf = xdr_getbuffer(*xu, &len);
    lua_pushlstring(L, buf, len);
    lua_pushnumber(L, len);
    return 2;
}

static int copyinto_buffer(lua_State* L) {
    const char* buf;
    size_t len;
    size_t size;
    xdr_ud* xu = _get_xdr_ud(L, 1);
    buf = luaL_checklstring(L, 2, &len);
    size = luaL_checknumber(L, 3);
    if (len > size) len = size;
    xdr_setbuffer(*xu, buf, len);
    return 0;
}

static const struct luaL_Reg mt_funcs[] = {
    {"reset", reset},
    {"pack_int8", pack_int8},
    {"pack_uint8", pack_uint8},
    {"pack_int16", pack_int16},
    {"pack_uint16", pack_uint16},
    {"pack_int32", pack_int32},
    {"pack_uint32", pack_uint32},
    {"pack_float", pack_float},
    {"pack_string", pack_string},
    {"pack_table", pack_table},
    {"unpack_int8", unpack_int8},
    {"unpack_uint8", unpack_uint8},
    {"unpack_int16", unpack_int16},
    {"unpack_uint16", unpack_uint16},
    {"unpack_int32", unpack_int32},
    {"unpack_uint32", unpack_uint32},
    {"unpack_float", unpack_float},
    {"unpack_string", unpack_string},
    {"unpack_table", unpack_table},
    {"get_remain", get_remain},
    {"get_buffer", get_buffer},
    {"set_buffer", set_buffer},
    {"copyout_buffer", copyout_buffer},
    {"copyinto_buffer", copyinto_buffer},
// compat
    {"clear", reset},
    {"pack_int", pack_int32},
    {"pack_uint", pack_uint32},
    {"pack_bool", pack_uint32},
    {"unpack_int", unpack_int32},
    {"unpack_uint", unpack_uint32},
    {"unpack_bool", unpack_uint32},
    {NULL, NULL},
};

static const struct luaL_Reg xdr_funcs[] = {
    {"new", create},
    {"newxdr", create},
    {"unpacker", unpacker},
    {NULL, NULL},
};

LUALIB_API int luaopen_liblxdr(lua_State* L) {
    luaL_newmetatable(L, XDR_MT);
    luaL_register(L, "", mt_funcs);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, destroy);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
    luaL_register(L, "", xdr_funcs);
    return 1;
}
