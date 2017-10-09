// puss_lua.c

#include "puss_lua.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "luaproxy_export.inl"

static void* module_require(lua_State* L, const char* m, void* ud);

int puss_ns_newmetatable(lua_State* L, int metatable_namespace, const char* metatable_name) {
	if( puss_namespace_rawget(L, metatable_namespace)==LUA_TTABLE )
		return 0;
	lua_pop(L, 1);
	lua_createtable(L, 0, 2);
	lua_pushstring(L, metatable_name);
	lua_setfield(L, -2, "__name");
	lua_pushvalue(L, -1);
	puss_namespace_rawset(L, metatable_namespace);
	return 1;
}

void* puss_ns_testudata(lua_State* L, int ud, int metatable_namespace) {
	void *p = lua_touserdata(L, ud);
	if( p && lua_getmetatable(L, ud) ) {
		puss_namespace_rawget(L, metatable_namespace);
		if( !lua_rawequal(L, -1, -2) )
			p = NULL;
		lua_pop(L, 2);
		return p;
	}
	return NULL;
}

static int typeerror (lua_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (luaL_getmetafield(L, arg, "__name") == LUA_TSTRING)
    typearg = lua_tostring(L, -1);  /* use the given type name */
  else if (lua_type(L, arg) == LUA_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = luaL_typename(L, arg);  /* standard name */
  msg = lua_pushfstring(L, "%s expected, got %s", tname, typearg);
  return luaL_argerror(L, arg, msg);
}

void* puss_ns_checkudata(lua_State* L, int ud, int metatable_namespace, const char* metatable_name) {
	void* p = puss_ns_testudata(L, ud, metatable_namespace);
	if (p == NULL) typeerror(L, ud, metatable_name);
	return p;
}

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

static const char* puss_ns_rawget_string(lua_State* L, int ns, const char* def) {
	const char* str = def;
	if( puss_namespace_rawget(L, ns)==LUA_TSTRING ) {
		str = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	return str;
}

static const char* module_app_path(lua_State* L) {
	return puss_ns_rawget_string(L, PUSS_NAMESPACE_APP_PATH, ".");
}

static int puss_module_require(lua_State* L) {
	const char* m = luaL_checkstring(L, 1);
	void* ud = lua_touserdata(L, 2);
	int top = lua_gettop(L);
	module_require(L, m, ud);
	return lua_gettop(L) - top;
}

static PussInterface puss_iface =
	{ __lua_proxy_export__
	, module_require
	, module_app_path
	, puss_rawget_ex
	, puss_pcall_stacktrace
	};

void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	puss_namespace_rawget(L, PUSS_NAMESPACE_PUSS);
	assert( lua_type(L, -1)==LUA_TTABLE );

	lua_pushstring(L, app_path);
	lua_pushvalue(L, -1);
	puss_namespace_rawset(L, PUSS_NAMESPACE_APP_PATH);
	lua_setfield(L, -2, "_path");	// puss._path

	lua_pushstring(L, app_name);
	lua_pushvalue(L, -1);
	puss_namespace_rawset(L, PUSS_NAMESPACE_APP_NAME);
	lua_setfield(L, -2, "_self");	// puss._self

	lua_pushstring(L, module_suffix);
	lua_pushvalue(L, -1);
	puss_namespace_rawset(L, PUSS_NAMESPACE_MODULE_SUFFIX);
	lua_setfield(L, -2, "_module_suffix");	// puss._module_suffix
}

static void* module_require(lua_State* L, const char* m, void* ud) {
	PussModuleInit f;
	const char* app_path = puss_ns_rawget_string(L, PUSS_NAMESPACE_APP_PATH, ".");
	const char* module_suffix = puss_ns_rawget_string(L, PUSS_NAMESPACE_MODULE_SUFFIX, ".so");
	const char* module_fname;
	luaL_Buffer B;
	lua_pop(L, 2);
	luaL_buffinit(L, &B);
	luaL_addstring(&B, app_path);
#ifdef _WIN32
	luaL_addstring(&B, "\\modules\\");
#else
	luaL_addstring(&B, "/modules/");
#endif
	luaL_addstring(&B, m);
	luaL_addstring(&B, module_suffix);
	luaL_pushresult(&B);
	module_fname = lua_tostring(L, -1);

	puss_rawget_ex(L, "package.loadlib");
	lua_pushstring(L, module_fname);
	lua_pushstring(L, "__puss_module_init__");
	lua_call(L, 2, 2);
	if( lua_type(L, -2)!=LUA_TFUNCTION )
		lua_error(L);
	f = (PussModuleInit)lua_tocfunction(L, -2);
	if( !f )
		luaL_error(L, "load module fetch init function failed!");
	return f(L, &puss_iface, ud);
}

PussInterface* puss_interface(void) {
	return &puss_iface;
}

void puss_lua_open(lua_State* L, int namespace_max_num) {
	int ridx;

	// check already open
	if (LUA_TTABLE == puss_namespace_rawget(L, PUSS_NAMESPACE_PUSS)) {
		lua_pop(L, 1);
		return;
	}
	lua_pop(L, 1);

	if( namespace_max_num < PUSS_NAMESPACE_MAX_NUM )
		luaL_error(L, "namespace_max_num, MUST >= PUSS_NAMESPACE_MAX_NUM"); 

	// check builtin 
	for( ridx=PUSS_NAMESPACE_PUSS; ridx<namespace_max_num; ++ridx ) {
		if( puss_namespace_rawget(L, ridx)!=LUA_TNIL ) {
			const char* err = "puss lua open failed: puss_lua_open MUST before any other lib open!!!\n";
			fprintf(stderr, err);
			abort();
		}

		lua_pop(L, 1);
		lua_pushboolean(L, 0);
		puss_namespace_rawset(L, ridx);
	}

	// puss namespace init
	lua_newtable(L);	// puss
	lua_pushvalue(L, -1);
	puss_namespace_rawset(L, PUSS_NAMESPACE_PUSS);
	puss_module_setup(L, ".", "puss", ".so");

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

	// module
	lua_pushcfunction(L, puss_module_require);
	lua_setfield(L, -2, "require");

	lua_pop(L, 1);
}

