// puss_lua.c

const char builtin_scripts[] = "-- puss_builtin.lua\n\n\n"
	"puss.dofile = function(name, env, ...)\n"
	"	local f, err = loadfile(name, 'bt', env or _ENV)\n"
	"	if not f then error(string.format('dofile (%s) failed: %s', name, err)) end\n"
	"	return f(...)\n"
	"end\n"
	"\n"
	"local _logerr = function(err) print(debug.traceback(err,2)); return err; end\n"
	"puss.logerr_handle = function(h) if type(h)=='function' then _logerr=h end; return _logerr end\n"
	"puss.trace_pcall = function(f, ...) return xpcall(f, _logerr, ...) end\n"
	"puss.trace_dofile = function(name, env, ...) return xpcall(puss.dofile, _logerr, name, env, ...) end\n"
	"\n"
	"local _loadmodule = function(name, env)\n"
	"	local fname = puss.filename_format(puss._path .. '/' .. name:gsub('%.', '/') .. '.lua', true)\n"
	"	local f, err = loadfile(fname, 'bt', env or _ENV)\n"
	"	if not f then error(string.format('load module(%s) failed: %s', name, err)) end\n"
	"	return f()\n"
	"end\n"
	"puss.loadmodule_handle = function(h) if type(h)=='function' then _loadmodule=h end; return _loadmodule end\n"
	"\n"
	"local modules = {}\n"
	"local modules_base_mt = { __index=_ENV }\n"
	"puss._modules = modules\n"
	"puss._modules_base_mt = modules_base_mt\n"
	"puss.import = function(name, reload)\n"
	"	local env = modules[name]\n"
	"	local interface\n"
	"	if env then\n"
	"		interface = getmetatable(env)\n"
	"		if not reload then return interface end\n"
	"	else\n"
	"		interface = {__name=name}\n"
	"		interface.__exports = interface\n"
	"		interface.__index = interface\n"
	"		local module_env = setmetatable(interface, modules_base_mt)\n"
	"		env = setmetatable({}, module_env)\n"
	"		modules[name] = env\n"
	"	end\n"
	"	_loadmodule(name, env)\n"
	"	return interface\n"
	"end\n"
	"puss.reload = function()\n"
	"	for name, env in pairs(modules) do\n"
	"		puss.trace_pcall(_loadmodule, name, env)\n"
	"	end\n"
	"end\n"
	"\n"
	;

#define _PUSS_MODULE_IMPLEMENT
#include "puss_module.h"
#include "puss_lua.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#include "puss_sys.inl"
#include "puss_pickle.inl"
#include "puss_debug.inl"

// consts
// 
static void puss_push_consts_table(lua_State* L) {
#ifdef LUA_USE_OPTIMIZATION_WITH_CONST
	if (lua_getfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST)!=LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST);
	}
#else
	lua_pushglobaltable(L);
#endif
}

// interface
// 
#define PUSS_KEY_INTERFACES		PUSS_KEY(interfaces)

static void puss_interface_register(lua_State* L, const char* name, void* iface) {
	int top = lua_gettop(L);
	if( !(name && iface) ) {
		luaL_error(L, "puss_interface_register(%s), name and iface MUST exist!", name);
	} else if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_INTERFACES)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, PUSS_KEY_INTERFACES);
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

static void* puss_interface_check(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	void* iface = ( name && lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_INTERFACES)==LUA_TTABLE
			&& lua_getfield(L, -1, name)==LUA_TUSERDATA ) ? lua_touserdata(L, -1) : NULL;
	if( !iface )
		luaL_error(L, "puss_interface_check(%s), iface not found!", name);
	lua_settop(L, top);
	return iface;
}

static PussInterface puss_interface = {
	puss_push_consts_table,
	puss_interface_register,
	puss_interface_check
};

static struct LuaProxy puss_lua_proxy = {
#define __LUAPROXY_SYMBOL(sym)	sym,
	#include "luaproxy.symbols"
#undef __LUAPROXY_SYMBOL
	__LUA_PROXY_FINISH_DUMMY__,
	&puss_interface
};

static int puss_lua_module_require(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	PussModuleInit f = NULL;
	if( lua_getfield(L, lua_upvalueindex(1), name) != LUA_TNIL )
		return 1;

	// local f, err = package.loadlib(module_filename, '__puss_module_init__')
	// 
	lua_pushvalue(L, lua_upvalueindex(2));	// package.loadlib
	lua_pushvalue(L, lua_upvalueindex(3));	// prefix
	lua_pushvalue(L, 1);					// name
	lua_pushvalue(L, lua_upvalueindex(4));	// suffix
	lua_concat(L, 3);
	lua_pushstring(L, "__puss_module_init__");
	lua_call(L, 2, 2);
	if( lua_type(L, -2)!=LUA_TFUNCTION )
		lua_error(L);
	f = (PussModuleInit)lua_tocfunction(L, -2);
	if( !f )
		luaL_error(L, "load module fetch init function failed!");
	lua_settop(L, 0);

	assert( strcmp(puss_lua_proxy.__lua_proxy_finish_dummy__, __LUA_PROXY_FINISH_DUMMY__)==0 );

	// __puss_module_init__()
	// puss_module_loaded[name] = <last return value> or true
	// 
	if( (f(L, &puss_lua_proxy)<=0) || lua_isnoneornil(L, -1) ) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
	}
	lua_pushvalue(L, -1);
	lua_setfield(L, lua_upvalueindex(1), name);
	return 1;
}

static void *_default_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}

static int _default_panic (lua_State *L) {
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                        lua_tostring(L, -1));
  return 0;  /* return to Lua to abort */
}

lua_State* puss_lua_newstate(int debug, lua_Alloc f, void* ud) {
	DebugEnv* env = NULL;
	lua_State* L;
	if( debug ) {
		env = debug_env_new(f ? f : _default_alloc, ud);
		if( !env ) return NULL;
		ud = env;
		f = _debug_alloc;
	} else {
		f = f ? f : _default_alloc;
	}
	L = lua_newstate(f, ud);
	if( L ) {
		lua_atpanic(L, &_default_panic);
	}
	if( env ) {
		env->main_state = L;
	}
	return L;
}

static luaL_Reg puss_methods[] =
	{ {"filename_format",	puss_lua_filename_format}
	, {"file_list",			puss_lua_file_list}
	, {"local_to_utf8",		puss_lua_local_to_utf8}
	, {"utf8_to_local",		puss_lua_utf8_to_local}
	, {"pack",				puss_lua_pack_lua}
	, {"unpack",			puss_lua_unpack_lua}
	, {NULL, NULL}
	};

static void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	int puss_index = lua_gettop(L) + 1;
	lua_newtable(L);	// puss

	// puss namespace init
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);

	// fprintf(stderr, "!!!puss_module_setup %s %s %s\n", app_path, app_name, module_suffix);

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);
	assert( lua_type(L, -1)==LUA_TTABLE );

	lua_pushstring(L, app_path ? app_path : ".");
	lua_setfield(L, puss_index, "_path");	// puss._path

	lua_pushstring(L, app_name ? app_name : "puss");
	lua_setfield(L, puss_index, "_self");	// puss._self

	lua_pushstring(L, PATH_SEP_STR);
	lua_setfield(L, puss_index, "_sep");	// puss._sep

	lua_pushstring(L, module_suffix ? module_suffix : ".so");
	lua_setfield(L, puss_index, "_module_suffix");	// puss._module_suffix

	lua_newtable(L);								// up[1]: modules
	lua_pushvalue(L, puss_index);
	lua_setfield(L, -2, "puss");	// puss modules["puss"] = puss-module
	lua_getglobal(L, "package");
	assert( lua_type(L, -1)==LUA_TTABLE );
	lua_getfield(L, -1, "loadlib");					// up[2]: package.loadlib
	assert( lua_type(L, -1)==LUA_TFUNCTION );
	lua_remove(L, -2);
	lua_getfield(L, puss_index, "_path");
	lua_pushstring(L, PATH_SEP_STR "modules" PATH_SEP_STR);
	lua_concat(L, 2);								// up[3]: module_prefix
	lua_getfield(L, puss_index, "_module_suffix");	// up[4]: module_suffix
	lua_pushcclosure(L, puss_lua_module_require, 4);
	lua_setfield(L, puss_index, "require");			// puss.require

	lua_settop(L, puss_index);
	luaL_setfuncs(L, puss_methods, 0);
	lua_setglobal(L, "puss");	// set to _G.puss

	// setup builtins
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts)-1, "@puss_builtin.lua") ) {
		lua_error(L);
	}
	lua_call(L, 0, 0);
}

void puss_lua_open(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	DebugEnv* env;

	// check already open
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS)==LUA_TTABLE ) {
		lua_setglobal(L, "puss");
		return;
	}
	lua_pop(L, 1);

	puss_module_setup(L, app_path, app_name, module_suffix);

	// debugger
	env = debug_env_fetch(L);
	if( env ) {
		puss_module_setup(env->debug_state, app_path, app_name, module_suffix);

		lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);
		lua_pushcfunction(L, lua_debugger_debug);
		lua_setfield(L, 1, "debug");	// puss.debug
		lua_pop(L, 1);
	}
}

void puss_lua_open_default(lua_State* L, const char* arg0, const char* module_suffix) {
	char pth[4096];
	int len;
	const char* app_path = NULL;
	const char* app_name = NULL;
#ifdef _WIN32
	len = (int)GetModuleFileNameA(0, pth, 4096);
#else
	len = readlink("/proc/self/exe", pth, 4096);
#endif
	if( len > 0 ) {
		pth[len] = '\0';
	} else {
		// try use argv[0]
		len = (int)strlen(arg0); 
		strcpy(pth, arg0);
	}

	for( --len; len>0; --len ) {
		if( pth[len]=='\\' || pth[len]=='/' ) {
			pth[len] = '\0';
			break;
		}
	}

	if( len > 0 ) {
		app_path = pth;
		app_name = pth+len+1;
	}

	puss_lua_open(L, app_path, app_name, module_suffix);
}

