// puss_lua.c

const char builtin_scripts[] = "-- puss_builtin.lua\n\n\n"
	"local sep = ...\n"
	"local strfmt = string.format\n"
	"\n"
	"local function puss_loadfile(name, env)\n"
	"	local path = strfmt('%s%s%s', puss._path, sep, name)\n"
	"	local f, err = loadfile(path, 'bt', env or _ENV)\n"
	"	if not f then return f, strfmt('load script(%s) failed: %s', path, err) end\n"
	"	return f\n"
	"end\n"
	"\n"
	"local function puss_dofile(name, env, ...)\n"
	"	local f, err = puss_loadfile(name, env)\n"
	"	if not f then\n"
	"		print(debug.traceback( strfmt('loadfile(%s) failed!', name) ))\n"
	"		error(err)\n"
	"	end\n"
	"	return f(...)\n"
	"end\n"
	"\n"
	"puss.loadfile = puss_loadfile\n"
	"puss.dofile = puss_dofile\n"
	"\n"
	"do\n"
	"	local _traceback = debug.traceback\n"
	"	local _logerr = print\n"
	"	local _logerr_handle = function(err) _logerr(_traceback(err,2)); return err;  end\n"
	"	puss.trace_pcall = function(f, ...) return xpcall(f, _logerr_handle, ...) end\n"
	"	puss.trace_dofile = function(name, env, ...) return xpcall(puss_dofile, _logerr_handle, name, env, ...) end\n"
	"end\n"
	"\n"
	;

#define _PUSS_MODULE_IMPLEMENT
#include "puss_module.h"
#include "puss_lua.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
	#define PATH_SEP	"\\"
#else
	#define PATH_SEP	"/"
#endif

#define PUSS_NAMESPACE(name)			"\x01" #name

#define PUSS_NAMESPACE_PUSS				PUSS_NAMESPACE(puss)
#define PUSS_NAMESPACE_MODULES_LOADED	PUSS_NAMESPACE(modules)
#define PUSS_NAMESPACE_INTERFACES		PUSS_NAMESPACE(interfaces)
#define PUSS_NAMESPACE_APP_PATH			PUSS_NAMESPACE(app_path)
#define PUSS_NAMESPACE_APP_NAME			PUSS_NAMESPACE(app_name)
#define PUSS_NAMESPACE_MODULE_SUFFIX	PUSS_NAMESPACE(module_suffix)

static int traceback(lua_State* L) {
	const char *msg = lua_tostring(L, 1);
	luaL_traceback(L, L, msg?msg:"(unknown error)", 1);
	return 1;
}

int puss_pcall_stacktrace(lua_State* L, int narg, int nres) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, traceback);
	lua_insert(L, base);  /* put it under chunk and args */
	status = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);  /* remove traceback function */
	return status;
}

int puss_rawget_ex(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	const char* ps = name;
	const char* pe = ps;
	int tp = LUA_TTABLE;

	lua_pushglobaltable(L);
	for( ; *pe; ++pe ) {
		if( *pe=='.' ) {
			if( tp!=LUA_TTABLE )
				goto not_found;
			lua_pushlstring(L, ps, pe-ps);
			tp = lua_rawget(L, -2);
			lua_remove(L, -2);
			ps = pe+1;
		}
	}

	if( ps!=pe && tp==LUA_TTABLE ) {
		lua_pushlstring(L, ps, pe-ps);
		tp = lua_rawget(L, -2);
		lua_remove(L, -2);
		return tp;
	}

not_found:
	lua_settop(L, top);
	lua_pushnil(L);
	tp = LUA_TNIL;
	return tp;
}

static int puss_simple_userdata_new(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	if( lua_isnoneornil(L, 2) ) {
		lua_newuserdata(L, 0);
	} else {
		lua_newuserdata(L, 0);
		lua_pushvalue(L, 2);
		lua_setuservalue(L, -2);
	}
	lua_pushvalue(L, 1);
	lua_setmetatable(L, -2);
	return 1;
}

static int puss_simple_userdata_set(lua_State* L) {
	luaL_checktype(L, 1, LUA_TUSERDATA);
	lua_setuservalue(L, 1);
	return 0;
}

static int puss_simple_userdata_get(lua_State* L) {
	if( lua_type(L, 1)==LUA_TUSERDATA )
		lua_getuservalue(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

static int wrapper_callk(lua_State *L, int status, lua_KContext ctx) {
	if( status==LUA_OK || status==LUA_YIELD )
		return lua_gettop(L) - (int)ctx;
	return lua_error(L);
}

static int function_except_wrapper(lua_State* L) {
	int status;
	int n = lua_gettop(L);
	lua_pushvalue(L, lua_upvalueindex(2));	// error handle
	lua_pushvalue(L, lua_upvalueindex(1));	// function
	lua_rotate(L, 1, 2);
	status = lua_pcallk(L, n, LUA_MULTRET, 1, (lua_KContext)1, wrapper_callk);
	return wrapper_callk(L, status, (lua_KContext)1);
}

static int puss_function_except_wrap(lua_State* L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);	// function
	luaL_checktype(L, 2, LUA_TFUNCTION);	// error handle
	lua_settop(L, 2);
	lua_pushcclosure(L, function_except_wrapper, 2);
	return 1;
}

static int function_wrapper(lua_State* L) {
	int n = lua_gettop(L);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	lua_callk(L, n, LUA_MULTRET, 0, wrapper_callk);
	return lua_gettop(L);
}

static int puss_function_wrap(lua_State* L) {
	if( lua_isnoneornil(L, 2) ) {
		// create wrap
		luaL_checktype(L, 1, LUA_TFUNCTION);	// function
		lua_settop(L, 1);
		lua_pushcclosure(L, function_wrapper, 1);
		return 1;
	}

	luaL_checktype(L, 2, LUA_TFUNCTION);	// wrapper
	luaL_argcheck(L, (lua_tocfunction(L, 2)==function_wrapper), 2, "need wrapper");

	if( lua_isnoneornil(L, 1) ) {
		// fetch raw function
		if( !lua_getupvalue(L, 2, 1) ) {
			return luaL_error(L, "fetch function from wrap failed!");
		}
	} else {
		// modify exist wrap
		luaL_checktype(L, 1, LUA_TFUNCTION);	// function
		lua_pushvalue(L, 1);
		if( !lua_setupvalue(L, 2, 1) ) {
			return luaL_error(L, "modify wrapper failed!");
		}
	}
	return 1;
}

static const char* lua_getfield_str(lua_State* L, const char* ns, const char* def) {
	const char* str = def;
	if( lua_getfield(L, LUA_REGISTRYINDEX, ns)==LUA_TSTRING ) {
		str = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	return str;
}

static int module_init_wrapper(lua_State* L);

static void puss_module_require(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	if( !name ) {
		luaL_error(L, "puss_module_require, module name MUST exist!");
	}
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushstring(L, name);
		lua_pushcclosure(L, module_init_wrapper, 1);
		lua_call(L, 0, 0);
	}
	lua_settop(L, top);
}

static void puss_interface_register(lua_State* L, const char* name, void* iface) {
	int top = lua_gettop(L);
	if( !(name && iface) ) {
		luaL_error(L, "puss_interface_register(%s), name and iface MUST exist!", name);
	}
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushlightuserdata(L, iface);
		lua_setfield(L, -2, name);
	} else if( lua_touserdata(L, -1)!=iface ) {
		luaL_error(L, "puss_interface_register(%s), iface already exist with different address!", name);
	}
	lua_settop(L, top);
}

static void* puss_interface_check(lua_State* L, const char* name) {
	void* iface = NULL;
	if( name ) {
		lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);
		lua_getfield(L, -1, name);
		iface = lua_touserdata(L, -1);
		lua_pop(L, 2);
	}
	if( !iface ) {
		luaL_error(L, "puss_interface_check(%s), iface not found!", name);
	}
	return iface;
}

static void puss_push_const_table(lua_State* L) {
#ifdef LUA_USE_OPTIMIZATION_WITH_CONST
	if( lua_getfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST);
	}
#else
	lua_pushglobaltable(L);
#endif
}

static const char* puss_app_path(lua_State* L) {
	return lua_getfield_str(L, PUSS_NAMESPACE_APP_PATH, ".");
}

static PussInterface puss_iface =
	{ puss_module_require
	, puss_interface_register
	, puss_interface_check
	, puss_push_const_table
	, puss_app_path
	, puss_rawget_ex
	, puss_pcall_stacktrace
	};

static int module_init_wrapper(lua_State* L) {
	const char* name = (const char*)lua_tostring(L, lua_upvalueindex(1));
	const char* app_path = lua_getfield_str(L, PUSS_NAMESPACE_APP_PATH, ".");
	const char* module_suffix = lua_getfield_str(L, PUSS_NAMESPACE_MODULE_SUFFIX, ".so");
	PussModuleInit f = NULL;
	assert( lua_gettop(L)==0 );

	// local f, err = package.loadlib(module_filename, '__puss_module_init__')
	// 
	puss_rawget_ex(L, "package.loadlib");
	{
		luaL_Buffer B;
		luaL_buffinit(L, &B);
		luaL_addstring(&B, app_path);
		luaL_addstring(&B, PATH_SEP "modules" PATH_SEP);
		luaL_addstring(&B, name);
		luaL_addstring(&B, module_suffix);
		luaL_pushresult(&B);
	}
	lua_pushstring(L, "__puss_module_init__");
	lua_call(L, 2, 2);
	if( lua_type(L, -2)!=LUA_TFUNCTION )
		lua_error(L);
	f = (PussModuleInit)lua_tocfunction(L, -2);
	if( !f )
		luaL_error(L, "load module fetch init function failed!");
	lua_pop(L, 2);
	assert( lua_gettop(L)==0 );

	// __puss_module_init__()
	// puss_module_loaded[name] = <last return value> or true
	// 
	if( (f(L, &puss_iface)<=0) || lua_isnoneornil(L, -1) ) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
	}

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);

	return 1;
}

static int puss_lua_module_require(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	if( lua_getfield(L, -1, name) != LUA_TNIL )
		return 1;
	lua_settop(L, 1);
	lua_pushcclosure(L, module_init_wrapper, 1);
	lua_call(L, 0, 1);
	return 1;
}

void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	// fprintf(stderr, "!!!puss_module_setup %s %s %s\n", app_path, app_name, module_suffix);

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS);
	assert( lua_type(L, -1)==LUA_TTABLE );

	puss_push_const_table(L);
	lua_setfield(L, -2, "_consts");	// puss._consts

	lua_pushstring(L, app_path);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_PATH);
	lua_setfield(L, -2, "_path");	// puss._path

	lua_pushstring(L, app_name);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_NAME);
	lua_setfield(L, -2, "_self");	// puss._self

	lua_pushstring(L, module_suffix);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULE_SUFFIX);
	lua_setfield(L, -2, "_module_suffix");	// puss._module_suffix
}

void puss_lua_open(lua_State* L) {
	// check already open
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS)==LUA_TTABLE ) {
		lua_setglobal(L, "puss");
		return;
	}
	lua_pop(L, 1);

	#define __LUAPROXY_SYMBOL(sym)	puss_iface.luaproxy.sym = sym;
		#include "luaproxy.symbols"
	#undef __LUAPROXY_SYMBOL

	// puss namespace init
	lua_newtable(L);	// puss
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS);
	puss_module_setup(L, ".", "puss", ".so");

	// puss modules["puss"] = puss-module
	lua_newtable(L);
	{
		lua_pushvalue(L, -2);
		lua_setfield(L, -2, "puss");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);

	// puss interfaces["PussInterface"] = puss_iface
	lua_newtable(L);
	{
		lua_pushlightuserdata(L, &puss_iface);
		lua_setfield(L, -2, "PussInterface");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);

	// set to _G.puss
	lua_pushvalue(L, -1);
	lua_setglobal(L, "puss");

	// puss.NULL
	lua_pushlightuserdata(L, NULL);
	lua_setfield(L, -2, "NULL");

	// puss.simple_userdata
	lua_pushcfunction(L, puss_simple_userdata_new);	lua_setfield(L, -2, "simple_userdata_new");
	lua_pushcfunction(L, puss_simple_userdata_set);	lua_setfield(L, -2, "simple_userdata_set");
	lua_pushcfunction(L, puss_simple_userdata_get);	lua_setfield(L, -2, "simple_userdata_get");

	// function wrappers
	lua_pushcfunction(L, puss_function_except_wrap);
	lua_setfield(L, -2, "function_except_wrap");
	lua_pushcfunction(L, puss_function_wrap);
	lua_setfield(L, -2, "function_wrap");

	// puss.require = _G.require = puss_lua_module_require
	lua_pushcfunction(L, puss_lua_module_require);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "require");
	lua_setfield(L, -2, "require");

	// setup builtins
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts)-1, "@puss_builtin.lua") ) {
		lua_error(L);
	}
	lua_pushstring(L, PATH_SEP);
	lua_call(L, 1, 0);

	lua_pop(L, 1);
}

