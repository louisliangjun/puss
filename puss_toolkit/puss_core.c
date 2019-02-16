// puss_core.c

static const char builtin_scripts[] = "-- puss_builtin.lua\n\n\n"
	"local puss = ...\n"
	"\n"
	"local _loadmodule = function(name, env)\n"
	"	local fname = puss.filename_format(puss._path .. '/' .. name:gsub('%.', '/') .. '.lua')\n"
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
	"	collectgarbage()\n"
	"end\n"
	"\n"
	;

#define _PUSS_IMPLEMENT
#include "puss_plugin.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#ifdef _MSC_VER
	#pragma warning(disable: 4996)		// VC++ depart functions warning
#endif

#include "puss_toolkit.h"

// interface
// 
#define PUSS_NAME_INTERFACES	"_@PussInterface@_"
#define PUSS_NAME_SIMPLESTATE	"_@PussSimpleState@_"

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

static int _default_error_handle(lua_State* L) {
	luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 0);
	if( lua_istable(L, -1) && lua_getfield(L, -1, "traceback")==LUA_TFUNCTION ) {
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
		lua_writestringerror("[PussError] %s\n", lua_tostring(L, -1));
		lua_settop(L, 1);
	} else {
		lua_settop(L, 1);
		lua_writestringerror("[PussError] %s\n", lua_tostring(L, -1));
	}
	return 1;
}

static int _puss_dostring(lua_State* L) {
	const char* script = luaL_checkstring(L, 1);
	if( lua_gettop(L) < 2 )
		lua_pushnil(L);
	if( !lua_isnil(L, 2) )
		luaL_checktype(L, 2, LUA_TTABLE);
	if( luaL_loadstring(L, script) )
		return luaL_error(L, "load script(%s) failed: %s", script, lua_tostring(L, -1));
	if( lua_istable(L, 2) ) {
		lua_pushvalue(L, 2);
		lua_setupvalue(L, -2, 1);
	}
	lua_replace(L, 2);
	lua_call(L, lua_gettop(L)-2, LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static int _puss_dofile(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	if( lua_gettop(L) < 2 )
		lua_pushnil(L);
	if( luaL_loadfile(L, name) )
		return luaL_error(L, "loadfile(%s) failed: %s", name, lua_tostring(L, -1));
	if( lua_istable(L, 2) ) {
		lua_pushvalue(L, 2);
		lua_setupvalue(L, -2, 1);
	}
	lua_replace(L, 2);
	lua_call(L, lua_gettop(L)-2, LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static int _puss_logerr_handle(lua_State* L) {
	if( lua_isfunction(L, 1) ) {
		lua_pushvalue(L, 1);
		__puss_config__.state_set_key(L, PUSS_KEY_ERROR_HANDLE);
	}
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	return 1;
}

static int _finish_trace_pcall(lua_State *L, int status, lua_KContext extra) {
	if (status != LUA_OK && status != LUA_YIELD) {
		lua_pushboolean(L, 0);
		lua_pushvalue(L, -2);
		return 2;
	}
	return lua_gettop(L) - 1;
}

static int _puss_trace_pcall(lua_State* L) {
	int status;
	int n = lua_gettop(L);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	lua_pushboolean(L, 1);
	lua_rotate(L, 1, 2);  // <err, true, f, ...>
	status = lua_pcallk(L, n - 1, LUA_MULTRET, 1, 0, _finish_trace_pcall);
	return _finish_trace_pcall(L, status, 0);
}

static int _puss_trace_dofile(lua_State* L) {
	int n = lua_gettop(L);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	lua_pushcfunction(L, _puss_dofile);
	lua_rotate(L, 1, 2);  // <err, _puss_dofile, ...>
	lua_pushboolean(L, lua_pcall(L, n, LUA_MULTRET, 1)==LUA_OK );
	lua_replace(L, 1);
	return lua_gettop(L);
}

static luaL_Reg core_methods[] =
	{ {"dostring", _puss_dostring}
	, {"dofile", _puss_dofile}
	, {"logerr_handle", _puss_logerr_handle}
	, {"trace_pcall", _puss_trace_pcall}
	, {"trace_dofile", _puss_trace_dofile}
	, {NULL, NULL}
	};

static int luaopen_puss(lua_State* L) {
	// check consts table
	if( puss_lua_get(L, PUSS_KEY_CONST_TABLE)!=LUA_TTABLE ) {
		lua_pushglobaltable(L);	// default use _G as CONST table
		__puss_config__.state_set_key(L, PUSS_KEY_CONST_TABLE);
	}
	lua_pop(L, 1);

	// check error handle
	if( puss_lua_get(L, PUSS_KEY_ERROR_HANDLE)!=LUA_TFUNCTION ) {
		lua_pushcfunction(L, _default_error_handle);	
		__puss_config__.state_set_key(L, PUSS_KEY_ERROR_HANDLE);
	}
	lua_pop(L, 1);

	// check already open
	if( puss_lua_get(L, PUSS_KEY_PUSS)==LUA_TTABLE )
		return 1;
	lua_pop(L, 1);

	lua_newtable(L);	// puss

	lua_pushvalue(L, -1);
	__puss_config__.state_set_key(L, PUSS_KEY_PUSS);

	// lua_writestringerror("!!!puss_module_setup %s %s %s\n", app_path, app_name, app_config);

	lua_pushstring(L, __puss_config__.app_path);
	lua_setfield(L, -2, "_path");		// puss._path

	lua_pushstring(L, __puss_config__.app_name);
	lua_setfield(L, -2, "_self");		// puss._self

	lua_newtable(L);					// up[1]: plugins
	lua_pushvalue(L, -2);
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
	lua_setfield(L, -2, "load_plugin");	// puss.load_plugin

	luaL_setfuncs(L, core_methods, 0);

	// reg modules
	puss_reg_puss_utils(L);
	puss_reg_simple_pickle(L);
	puss_reg_simple_luastate(L);
	puss_reg_async_service(L);
	puss_reg_thread_service(L);
	return 1;
}

int puss_load_builtins(lua_State* L) {
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts) - 1, "@puss_builtin.lua") )
		lua_error(L);
	luaL_requiref(L, "puss", luaopen_puss, 1);
	lua_call(L, 1, 0);
	return 0;
}

// default puss toolkit sink

static void *_default_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
	(void)ud; (void)osize;  /* not used */
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

static int _default_panic(lua_State *L) {
	lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));
	return 0;  /* return to Lua to abort */
}

static lua_State* _default_puss_state_new(void) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	lua_pushcfunction(L, puss_load_builtins);
	if (lua_pcall(L, 0, 0, 0)) {
		lua_writestringerror("[Puss] load builtins failed: %s\n", lua_tostring(L, -1));
		lua_close(L);
		L = NULL;
	}
	return L;
}

static int _default_puss_key_get(lua_State* L, PussToolkitKey key) {
	char* r = (char*)&__puss_config__;
	return lua_rawgetp(L, LUA_REGISTRYINDEX, r+key);
}

static void _default_puss_key_set(lua_State* L, PussToolkitKey key) {
	char* r = (char*)&__puss_config__;
	lua_rawsetp(L, LUA_REGISTRYINDEX, r+key);
}

#define DEFAULT_PATH_SIZE	4096

static char _default_app_config[16] = { 0 };
static char _default_arg0[] = { '.', '/', 'p', 'u', 's', 's', 0 };
static char* _default_args[2] = { _default_arg0, NULL };

PussConfig	__puss_config__ =
	{ 1
	, _default_args

	, "."
	, "puss"
	, _default_app_config

	, _default_alloc
	, _default_panic

	, _default_puss_state_new
	, _default_puss_key_get
	, _default_puss_key_set
	};

static char _default_path[DEFAULT_PATH_SIZE] = { 0 };

void puss_config_init(int argc, char* argv[]) {
	char* pth = _default_path;
	int len = 0;
	char* p;

	__puss_config__.app_argc = argc;
	__puss_config__.app_argv = argv;

#ifdef _WIN32
	len = (int)GetModuleFileNameA(0, pth, DEFAULT_PATH_SIZE-32);
	for( p=pth+len-1; p>pth; --p ) {
		if( *p=='\\' || *p=='/' )
			break;
	}
	strcpy(p, "\\plugins\\");
	SetDllDirectoryA(pth);

	len = (int)GetModuleFileNameA(0, pth, DEFAULT_PATH_SIZE-32);
	for( p=pth+len-1; p>pth; --p ) {
		if( *p=='\\' )
			*p = '/';
	}
#else
	len = readlink("/proc/self/exe", pth, DEFAULT_PATH_SIZE-32);
	if( len > 0 ) {
		pth[len] = '\0';
	} else {
		// try use argv[0]
		len = (int)strlen(__puss_config__.app_argv[0]);
		if( len >= DEFAULT_PATH_SIZE )	exit(1);
		strcpy(pth, __puss_config__.app_argv[0]);
	}
#endif

	for( --len; len>0; --len ) {
		if( pth[len]=='/' ) {
			pth[len] = '\0';
			break;
		}
	}

	if( len <= 0 )
		return;

	__puss_config__.app_path = pth;
	__puss_config__.app_name = pth+len+1;

	// fetch config from filename <app-CONFIG>, in win32 <app-CONFIG.exe>
	{
		char* ps = pth + len + 1;
		char* pe = ps + strlen(ps);
		size_t n;

#ifdef _WIN32
		// skip .exe
		for( p=pe-1; p>ps; --p ) {
			if( *(p-1)=='.' && (pe-p)==3 && (p[0]=='e' || p[0]=='E') && (p[1]=='x' || p[1]=='X') && (p[2]=='e' || p[2]=='E') ) {
				pe = --p;
				break;
			}
		}
#endif
		for( p=pe-1; p>ps; --p ) {
			if( *p != '-' )
				continue;

			n = pe - p;
			if( n >= sizeof(_default_app_config) ) {
				// too long CONFIG, use default config ""
				break;
			}

			memcpy(_default_app_config, p, n);
			_default_app_config[n] = '\0';
			__puss_config__.app_config = _default_app_config;
			break;
		}
	}
}