// nuklear_lua.inl

// usage :
/*
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "nuklear.h"
#include "nuklear_lua.inl"
*/

//----------------------------------------------------------

#define LUA_NK_LIB_NAME	"puss_nuklear_lib"

// apis
#define USE_LUA_NK_API_IMPLEMENT
	#include "nuklear_lua_apis.inl"
#undef USE_LUA_NK_API_IMPLEMENT

static luaL_Reg nuklera_lua_apis[] = {
#define USE_LUA_NK_API_REGISTER
	#include "nuklear_lua_apis.inl"
#undef USE_LUA_NK_API_REGISTER

	{ NULL, NULL }
};

static int lua_new_nk_lib(lua_State* L) {
	if( lua_getfield(L, LUA_REGISTRYINDEX, LUA_NK_LIB_NAME)==LUA_TTABLE )
		return 0;
	lua_pop(L, 1);

	lua_newtable(L);
	luaL_setfuncs(L, nuklera_lua_apis, 0);
#define _lua_nk_reg_filter(filter)	lua_push_nk_plugin_filter(L, filter); lua_setfield(L, -2, #filter)
	_lua_nk_reg_filter(nk_filter_default);
	_lua_nk_reg_filter(nk_filter_ascii);
	_lua_nk_reg_filter(nk_filter_float);
	_lua_nk_reg_filter(nk_filter_decimal);
	_lua_nk_reg_filter(nk_filter_hex);
	_lua_nk_reg_filter(nk_filter_oct);
	_lua_nk_reg_filter(nk_filter_binary);
#undef _lua_nk_reg_filter
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_NK_LIB_NAME);
	return 1;
}

