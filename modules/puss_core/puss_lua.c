// puss_lua.c

const char builtin_scripts[] = "-- puss_builtin.lua\n\n\n"
	"local sep = ...\n"
	"\n"
	"local function puss_loadfile(name, env)\n"
	"	local path = string.format('%s%s%s', puss._path, sep, name)\n"
	"	local f, err = loadfile(path, 'bt', env or _ENV)\n"
	"	if not f then return f, string.format('load script(%s) failed: %s', path, err) end\n"
	"	return f\n"
	"end\n"
	"\n"
	"local function puss_dofile(name, env, ...)\n"
	"	local f, err = puss_loadfile(name, env)\n"
	"	if not f then\n"
	"		print(debug.traceback( string.format('loadfile(%s) failed!', name) ))\n"
	"		error(err)\n"
	"	end\n"
	"	return f(...)\n"
	"end\n"
	"\n"
	"puss.loadfile = puss_loadfile\n"
	"puss.dofile = puss_dofile\n"
	"\n"
	"do\n"
	"	local logerr_handle = function(err) print(debug.traceback(err,2)); return err;  end\n"
	"	puss.trace_pcall = function(f, ...) return xpcall(f, logerr_handle, ...) end\n"
	"	puss.trace_dofile = function(name, env, ...) return xpcall(puss_dofile, logerr_handle, name, env, ...) end\n"
	"end\n"
	"\n"
	;

#define _PUSS_MODULE_IMPLEMENT
#include "puss_module.h"
#include "puss_lua.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
	#define PATH_SEP	"\\"
#else
	#include <unistd.h>
	#define PATH_SEP	"/"
#endif

#include "puss_debug.inl"

#define PUSS_NAMESPACE(name)			"[" #name "]"

#define PUSS_NAMESPACE_PUSS				PUSS_NAMESPACE(puss)
#define PUSS_NAMESPACE_MODULES_LOADED	PUSS_NAMESPACE(modules)
#define PUSS_NAMESPACE_INTERFACES		PUSS_NAMESPACE(interfaces)
#define PUSS_NAMESPACE_APP_PATH			PUSS_NAMESPACE(app_path)
#define PUSS_NAMESPACE_APP_NAME			PUSS_NAMESPACE(app_name)
#define PUSS_NAMESPACE_MODULE_SUFFIX	PUSS_NAMESPACE(module_suffix)

int puss_get_value(lua_State* L, const char* name) {
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
	if( !name ) {
		luaL_error(L, "puss_module_require, module name MUST exist!");
	}
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushstring(L, name);
		lua_pushcclosure(L, module_init_wrapper, 1);
		lua_call(L, 0, 1);
	}
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
	, puss_get_value

	, puss_debug_command

	};

static int module_init_wrapper(lua_State* L) {
	const char* name = (const char*)lua_tostring(L, lua_upvalueindex(1));
	const char* app_path = lua_getfield_str(L, PUSS_NAMESPACE_APP_PATH, ".");
	const char* module_suffix = lua_getfield_str(L, PUSS_NAMESPACE_MODULE_SUFFIX, ".so");
	PussModuleInit f = NULL;
	assert( lua_gettop(L)==0 );

	// local f, err = package.loadlib(module_filename, '__puss_module_init__')
	// 
	puss_get_value(L, "package.loadlib");
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

static void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	// fprintf(stderr, "!!!puss_module_setup %s %s %s\n", app_path, app_name, module_suffix);

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS);
	assert( lua_type(L, -1)==LUA_TTABLE );

	puss_push_const_table(L);
	lua_setfield(L, -2, "_consts");	// puss._consts

	lua_pushstring(L, app_path ? app_path : ".");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_PATH);
	lua_setfield(L, -2, "_path");	// puss._path

	lua_pushstring(L, app_name ? app_name : "puss");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_NAME);
	lua_setfield(L, -2, "_self");	// puss._self

	lua_pushstring(L, module_suffix ? module_suffix : ".so");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULE_SUFFIX);
	lua_setfield(L, -2, "_module_suffix");	// puss._module_suffix
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

void puss_lua_open(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
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
	puss_module_setup(L, app_path, app_name, module_suffix);

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

	// puss.require = puss_lua_module_require
	lua_pushcfunction(L, puss_lua_module_require);
	lua_setfield(L, -2, "require");

	// setup builtins
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts)-1, "@puss_builtin.lua") ) {
		lua_error(L);
	}
	lua_pushstring(L, PATH_SEP);
	lua_call(L, 1, 0);

	// debugger
	if( lua_getallocf(L, NULL)==_debug_alloc ) {
		luaL_newlib(L, lua_debug_methods);
		lua_setfield(L, 1, "debug");	// puss.debug

		puss_push_const_table(L);
	#define _reg(e)	lua_pushinteger(L, e);	lua_setfield(L, -2, #e)
		_reg(PUSS_DEBUG_EVENT_ATTACHED);
		_reg(PUSS_DEBUG_EVENT_UPDATE);
		_reg(PUSS_DEBUG_EVENT_HOOK_COUNT);
		_reg(PUSS_DEBUG_EVENT_BREAKED_BEGIN);
		_reg(PUSS_DEBUG_EVENT_BREAKED_UPDATE);
		_reg(PUSS_DEBUG_EVENT_BREAKED_END);
	#undef _reg
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
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

