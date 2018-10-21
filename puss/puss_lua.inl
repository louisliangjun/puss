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

#define _PUSS_IMPLEMENT
#include "puss_plugin.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#define PUSS_KEY(name)			"[" #name "]"

#define PUSS_KEY_PUSS			PUSS_KEY(puss)

#ifdef _MSC_VER
	#pragma warning(disable: 4996)		// VC++ depart functions warning
#endif

#include "puss_sys.inl"
#include "puss_pickle.inl"
#include "puss_debug.inl"

// interface
// 
#define PUSS_KEY_INTERFACES		PUSS_KEY(interfaces)

static void* puss_interface_check(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	void* iface = ( name && lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_INTERFACES)==LUA_TTABLE
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

// consts
// 
static void puss_push_consts_table(lua_State* L) {
	lua_pushglobaltable(L);
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
	lua_pushvalue(L, lua_upvalueindex(4));	// prefix
	lua_concat(L, 3);
}

static int puss_lua_plugin_load(lua_State* L) {
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
		lua_pushfstring(L, "plugin: ");
		_push_plugin_filename(L);
		lua_concat(L, 3);
		lua_error(L);
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

static void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* app_config) {
	int puss_index = lua_gettop(L) + 1;
	lua_newtable(L);	// puss

	// puss namespace init
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);

	// fprintf(stderr, "!!!puss_module_setup %s %s %s\n", app_path, app_name, app_config);

	lua_pushstring(L, app_path ? app_path : ".");
	lua_setfield(L, puss_index, "_path");	// puss._path

	lua_pushstring(L, app_name ? app_name : "puss");
	lua_setfield(L, puss_index, "_self");	// puss._self

	lua_pushstring(L, PATH_SEP_STR);
	lua_setfield(L, puss_index, "_sep");	// puss._sep

	lua_newtable(L);								// up[1]: plugins
	lua_pushvalue(L, puss_index);
	lua_setfield(L, -2, "puss");	// puss plugins["puss"] = puss-self
	lua_getglobal(L, "package");
	assert( lua_type(L, -1)==LUA_TTABLE );
	lua_getfield(L, -1, "loadlib");					// up[2]: package.loadlib
	assert( lua_type(L, -1)==LUA_TFUNCTION );
	lua_remove(L, -2);
	lua_getfield(L, puss_index, "_path");
	lua_pushstring(L, PATH_SEP_STR "plugins" PATH_SEP_STR);
	lua_concat(L, 2);								// up[3]: plugin_prefix
#ifdef _WIN32
	lua_pushfstring(L, "%s.dll", app_config);		// up[4]: plugin_suffix
#else
	lua_pushfstring(L, "%s.so", app_config);
#endif
	lua_pushcclosure(L, puss_lua_plugin_load, 4);
	lua_setfield(L, puss_index, "load_plugin");		// puss.load_plugin

	lua_settop(L, puss_index);
	luaL_setfuncs(L, puss_methods, 0);
	lua_setglobal(L, "puss");	// set to _G.puss

	// setup builtins
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts)-1, "@puss_builtin.lua") ) {
		lua_error(L);
	}
	lua_call(L, 0, 0);
}

static void puss_lua_open(lua_State* L, const char* app_path, const char* app_name, const char* app_config) {
	DebugEnv* env;

	// check already open
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS)==LUA_TTABLE ) {
		lua_setglobal(L, "puss");
		return;
	}
	lua_pop(L, 1);

	puss_module_setup(L, app_path, app_name, app_config);

	// debugger
	env = debug_env_fetch(L);
	if( env ) {
		puss_module_setup(env->debug_state, app_path, app_name, app_config);

		lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);
		lua_pushcfunction(L, lua_debugger_debug);
		lua_setfield(L, 1, "debug");	// puss.debug
		lua_pop(L, 1);
	}
}

static void puss_lua_open_default(lua_State* L, const char* arg0) {
	char pth[4096];
	int len;
	const char* app_path = NULL;
	const char* app_name = NULL;
	char app_config[16] = { 0 };
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

	{

		const char* ps = app_name;
		const char* pe = app_name + strlen(app_name);
		const char* p;

		// fetch config from filename <app.CONFIG>, in win32 <app.CONFIG.exe>
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
			if( *p == '.' ) {
				size_t n = pe - p;
				if( n >= sizeof(app_config) ) {
					// too long CONFIG, use default config ""
				} else {
					memcpy(app_config, p, n);
					app_config[n] = '\0';
				}
				break;
			}
		}
	}

	puss_lua_open(L, app_path, app_name, app_config);
}

