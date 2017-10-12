// puss_lua.h

#ifndef _INC_PUSS_LUA_H_
#define _INC_PUSS_LUA_H_

#include "puss_module.h"

PUSS_DECLS_BEGIN

// NOTICE：this will use lua register table [LUA_RIDX_LAST+1 ... namespace_max_num]
// so, puss_lua_open() MUST before any other lib open!!!
// 
void puss_lua_open(lua_State* L, int namespace_max_num);	// namespace_max_num, MUST >= PUSS_NAMESPACE_MAX_NUM

#define PUSS_METATABLE_(T)		PUSS_METATABLE_##T
#define PUSS_METATABLE_NAME_(T)	#T

typedef enum _PussBuiltinNamespace
	{ PUSS_NAMESPACE_PUSS = LUA_RIDX_LAST + 1
	, PUSS_NAMESPACE_MODULES_LOADED
	, PUSS_NAMESPACE_APP_PATH
	, PUSS_NAMESPACE_APP_NAME
	, PUSS_NAMESPACE_MODULE_SUFFIX
	, PUSS_NAMESPACE_PICKLE_CACHE
	, PUSS_NAMESPACE_PICKLE_REF_TABLE
	, PUSS_NAMESPACE_PICKLE_MAGIC
	, PUSS_NAMESPACE_FFI_CACHE_PUSH
	, PUSS_NAMESPACE_FFI_CACHE_TRACE

	// metatables namespace
	//, PUSS_METATABLE_(SimpleSocket)

	, PUSS_NAMESPACE_MAX_NUM	// MUST LAST
	} PussBuiltinNamespace;

#define puss_namespace_rawset(L, ns)	lua_rawseti((L), LUA_REGISTRYINDEX, (ns))
#define puss_namespace_rawget(L, ns)	lua_rawgeti((L), LUA_REGISTRYINDEX, (ns))

int     puss_ns_newmetatable(lua_State* L, int metatable_namespace, const char* metatable_name);
void*   puss_ns_testudata(lua_State* L, int ud, int metatable_namespace);
void*   puss_ns_checkudata(lua_State* L, int ud, int metatable_namespace, const char* metatable_name);

#define puss_getmetatable(L,T)		puss_namespace_rawget((L), PUSS_METATABLE_(T))
#define puss_newmetatable(L,T)		puss_ns_newmetatable((L), PUSS_METATABLE_(T), PUSS_METATABLE_NAME_(T))
#define puss_setmetatable(L,T)		do { puss_getmetatable((L), T); lua_setmetatable((L), -2); } while(0)
#define puss_testudata(L,ud,T)		((T*)puss_ns_testudata((L), (ud), PUSS_METATABLE_(T)))
#define puss_checkudata(L,ud,T)		((T*)puss_ns_checkudata((L), (ud), PUSS_METATABLE_(T), PUSS_METATABLE_NAME_(T)))
#define puss_newuserdata(L,T)		((T*)lua_newuserdata(L, sizeof(T)))

// usage : for example call system.xxfuncion(int, str) return int
/*
	puss_rawget_ex(L, "system.xxfunction");	// push function
	lua_pushinteger(L, 333);					// push arg1 int
	lua_pushstring(L, "abc");					// push arg2 str
	if( puss_pcall_ex(L, 2, 1) ) {				// call function, 2 args, 0 return value
		// print script error
		printf( "call script error: %s\n", lua_tostring(L, -1) );
		lua_pop(L, 1);							// pop error
	} else {
		// print succeed return value
		printf( "return %d\n", lua_tointeger(L, -1) );
		lua_pop(L, 1);							// pop return value
	}
*/
int  puss_pcall_stacktrace(lua_State* L, int n, int r);

#ifdef _DEBUG
	#define	puss_pcall_ex(L,n,r)	puss_pcall_stacktrace((L),(n),(r))
#else
	#define puss_pcall_ex(L,n,r)	lua_pcall((L),(n),(r),0)
#endif

// push value to L, push nil if not find
// 
//	for example : puss_rawget_ex(L, "system.test.foo")
// 
int puss_rawget_ex(lua_State* L, const char* name);

// puss modules
// 
void	puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_H_

