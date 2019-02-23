// puss_plugin.inl

#define _PUSS_IMPLEMENT
#include "puss_plugin.h"

#include "puss_toolkit.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// interface
// 
#define PUSS_NAME_INTERFACES	"_@PussInterface@_"

static void* puss_interface_check(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	void* iface = ( name && lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAME_INTERFACES)==LUA_TTABLE
			&& lua_getfield(L, -1, name)==LUA_TUSERDATA ) ? lua_touserdata(L, -1) : NULL;
	if( !iface )
		luaL_error(L, "puss_interface_check(%s), iface not found!", name);
	lua_settop(L, top);
	return iface;
}

static void puss_interface_register(lua_State* L, const char* name, void* iface) {
	int top = lua_gettop(L);
	if( !(name && iface) ) {
		luaL_error(L, "puss_interface_register(%s), name and iface MUST exist!", name);
	} else if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAME_INTERFACES)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAME_INTERFACES);
	}
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushlightuserdata(L, iface);
		lua_setfield(L, -2, name);
	} else if( lua_touserdata(L, -1)!=iface ) {
		luaL_error(L, "puss_interface_register(%s), iface already exist with different address!", name);
	}
	lua_settop(L, top);
}

// consts
// 
static void puss_push_consts_table(lua_State* L) {
	puss_lua_get(L, PUSS_KEY_CONST_TABLE);
}

static PussInterface puss_interface =
	{
		{
#define __LUAPROXY_SYMBOL(sym)	sym,
		#include "luaproxy.symbols"
#undef __LUAPROXY_SYMBOL
		__LUA_PROXY_FINISH_DUMMY__
		}
	, puss_interface_check
	, puss_interface_register
	, puss_push_consts_table
	};

static void _push_plugin_filename(lua_State* L) {
	lua_pushvalue(L, lua_upvalueindex(3));	// prefix
	lua_pushvalue(L, 1);					// name
	lua_pushvalue(L, lua_upvalueindex(4));	// suffix
	lua_concat(L, 3);
}

static int _puss_lua_plugin_load(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	PussPluginInit f = NULL;
	if( lua_getfield(L, lua_upvalueindex(1), name) != LUA_TNIL )
		return 1;

	// local f, err = package.loadlib(plugin_filename, '__puss_plugin_init__')
	// 
	lua_pushvalue(L, lua_upvalueindex(2));	// package.loadlib
	_push_plugin_filename(L);
	lua_pushstring(L, "__puss_plugin_init__");
	lua_call(L, 2, 2);
	if( lua_type(L, -2)!=LUA_TFUNCTION ) {
		_push_plugin_filename(L);
		luaL_error(L, "load plugin(%s) error:%s", lua_tostring(L, -1), lua_tostring(L, -2));
	}
	f = (PussPluginInit)lua_tocfunction(L, -2);
	if( !f )
		luaL_error(L, "load plugin fetch init function failed!");
	lua_settop(L, 0);

	assert( strcmp(puss_interface.lua_proxy.__lua_proxy_finish_dummy__, __LUA_PROXY_FINISH_DUMMY__)==0 );

	// __puss_plugin_init__()
	// puss_plugin_loaded[name] = <last return value> or true
	// 
	if( (f(L, &puss_interface)<=0) || lua_isnoneornil(L, -1) ) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
	}
	lua_pushvalue(L, -1);
	lua_setfield(L, lua_upvalueindex(1), name);
	return 1;
}

static int puss_plugin_loader_reg(lua_State* L) {
	lua_newtable(L);					// up[1]: plugins
	lua_pushvalue(L, -2);				// puss
	lua_setfield(L, -2, "puss");		// puss plugins["puss"] = puss-self

	luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 0);
	assert( lua_type(L, -1)==LUA_TTABLE );
	lua_getfield(L, -1, "loadlib");		// up[2]: package.loadlib
	assert( lua_type(L, -1)==LUA_TFUNCTION );
	lua_remove(L, -2);

	lua_pushfstring(L, "%s/plugins/", __puss_config__.app_path);	// up[3]: plugin_prefix
#ifdef _WIN32
	lua_pushfstring(L, "%s.dll", __puss_config__.app_config);		// up[4]: plugin_suffix win32
#else
	lua_pushfstring(L, "%s.so", __puss_config__.app_config);		// up[4]: plugin_suffix unix
#endif
	lua_pushcclosure(L, _puss_lua_plugin_load, 4);
	lua_setfield(L, 1, "load_plugin");	// puss.load_plugin
	return 0;
}
