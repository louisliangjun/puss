// puss_toolkit.inl

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#define lua_assert	assert
#define _PUSS_IMPLEMENT
#include "puss_plugin.h"

#include "puss_toolkit.h"

#include "puss_debug.inl"

static void *_default_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
	(void)ud; (void)osize;  /* not used */
	if( nsize==0 ) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

static int _default_panic (lua_State *L) {
	lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));
	return 0;  /* return to Lua to abort */
}

static lua_State* puss_lua_newstate(int debug, lua_Alloc f, void* ud) {
	DebugEnv* dbg = NULL;
	lua_State* L;
	if( debug ) {
		dbg = lua_debugger_new(f ? f : _default_alloc, ud);
		if( !dbg ) return NULL;
		ud = dbg;
		f = _debug_alloc;
	} else {
		f = f ? f : _default_alloc;
	}
	L = lua_newstate(f, ud);
	if( L ) {
		lua_atpanic(L, &_default_panic);
	}
	if( dbg ) {
		dbg->main_state = L;
	}
	return L;
}

static int _puss_lua_state_new(lua_State* owner) {
	PussSetup setup = (PussSetup)lua_touserdata(owner, lua_upvalueindex(1));
	const char* app_path = lua_tostring(owner, lua_upvalueindex(2));
	const char* app_name = lua_tostring(owner, lua_upvalueindex(3));
	const char* app_config = lua_tostring(owner, lua_upvalueindex(4));
	lua_State* L = puss_lua_newstate(debug_env_fetch(owner)!=NULL, _default_alloc, NULL);
	(*setup)(L, app_path, app_name, app_config);
	lua_pushlightuserdata(owner, L);
	return 1;
}

static void puss_lua_setup(lua_State* L, const char* app_path, const char* app_name, const char* app_config) {
	// support: puss.lua_newstate
	{
		lua_pushlightuserdata(L, puss_lua_setup);
		lua_pushstring(L, app_path);
		lua_pushstring(L, app_name);
		lua_pushstring(L, app_config);
		lua_pushcclosure(L, _puss_lua_state_new, 4);
		lua_setfield(L, LUA_REGISTRYINDEX, PUSS_KEY_NEWSTATE);
	}

	luaL_openlibs(L);
	puss_lua_setup_base(L, app_path, app_name, app_config);

	// support: puss.debug
	{
		DebugEnv* dbg = debug_env_fetch(L);
		if( dbg ) {
			lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);
			puss_lua_setup_base(dbg->debug_state, app_path, app_name, app_config);
			lua_pushcfunction(L, lua_debugger_debug);
			lua_setfield(L, -2, "debug");	// puss.debug
			lua_pop(L, 1);
		}
	}
}
