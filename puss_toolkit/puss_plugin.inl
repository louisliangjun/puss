// puss_plugin.inl

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

	, puss_cobject_checkudata
	, puss_cobject_testudata
	, puss_cobject_check
	, puss_cobject_test
	, puss_cobject_stack_get
	, puss_cobject_stack_set
	, puss_cobject_set_bool
	, puss_cobject_set_int
	, puss_cobject_set_num
	, puss_cobject_set_ptr
	, puss_cmonitor_reset
	, puss_cstack_formular_reset
	, puss_cformular_reset
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

#ifdef _PUSS_PLUGIN_BASIC_PROTECT
	#include "plugin_protect.inl"
#endif

#ifdef _PUSS_PLUGIN_USE_MEMORY_PE
	#include "mempe_lua.inl"
#endif

static int puss_plugin_loader_reg(lua_State* L) {
	int top = lua_gettop(L);
#ifdef _PUSS_PLUGIN_BASIC_PROTECT
	puss_protect_ensure();
#endif

	// top + 1
	lua_newtable(L);					// plugins
	lua_pushvalue(L, -2);				// puss
	lua_setfield(L, -2, "puss");		// puss plugins["puss"] = puss-self

	// top + 2
	lua_pushfstring(L, "%s/plugins/", __puss_config__.app_path);
	lua_pushvalue(L, -1);
	lua_setfield(L, 1, "_plugin_prefix");	// puss._plugin_prefix

	// top + 3
#ifdef _WIN32
	lua_pushfstring(L, "%s.dll", __puss_config__.app_config);
#else
	lua_pushfstring(L, "%s.so", __puss_config__.app_config);
#endif
	lua_pushvalue(L, -1);
	lua_setfield(L, 1, "_plugin_suffix");	// puss._plugin_suffix

	// puss.load_plugin
	{
		lua_pushvalue(L, top+1);			// up[1]: puss.plugins

		luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 0);
		assert( lua_type(L, -1)==LUA_TTABLE );
		lua_getfield(L, -1, "loadlib");		// up[2]: package.loadlib
		assert( lua_type(L, -1)==LUA_TFUNCTION );
		lua_remove(L, -2);

		lua_pushvalue(L, top + 2);			// up[3]: puss.plugin_prefix
		lua_pushvalue(L, top + 3);			// up[4]: puss.plugin_suffix
		lua_pushcclosure(L, _puss_lua_plugin_load, 4);
		lua_setfield(L, 1, "load_plugin");	// puss.load_plugin
	}

#ifdef _WIN32
  #ifdef _PUSS_PLUGIN_USE_MEMORY_PE
	lua_pushvalue(L, top + 1);	// up[1]: puss.plugins
	lua_pushvalue(L, top + 2);	// up[2]: puss.plugin_prefix
	lua_pushvalue(L, top + 3);	// up[3]: puss.plugin_suffix
	#ifdef _PUSS_PLUGIN_BASIC_PROTECT
		lua_pushlightuserdata(L, &puss_protect_interface);	// up[4]: puss_interface, protect support
	#else
		lua_pushlightuserdata(L, &puss_interface);			// up[4]: puss_interface
	#endif
	lua_pushcclosure(L, _puss_lua_plugin_load_mpe, 4);
	lua_setfield(L,1, "load_plugin_mpe");	// puss.load_plugin_mpe
  #endif
#endif

	lua_settop(L, top);
	return 0;
}
