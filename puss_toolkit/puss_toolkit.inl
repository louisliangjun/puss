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

static lua_State* puss_lua_newstate(void) {
	lua_Alloc alloc = _default_alloc;
	DebugEnv* dbg = NULL;
	lua_State* L;
	if( __puss_toolkit_sink__.app_debug_level ) {
		dbg = lua_debugger_new(_default_alloc, NULL);
		if( !dbg ) return NULL;
		alloc = _debug_alloc;
	}
	L = lua_newstate(alloc, dbg);
	if( L ) {
		lua_atpanic(L, &_default_panic);
		luaL_openlibs(L);
		lua_pushcfunction(L, puss_lua_init);
		if( lua_pcall(L, 0, 0, 0) ) {
			fprintf(stderr, "[Puss] init failed: %s\n", lua_tostring(L, -1));
			lua_close(L);
			L = NULL;
		}
	}
	lua_debugger_attach(dbg, L);
	return L;
}
