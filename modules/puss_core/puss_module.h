// puss_lua_module.h

#ifndef _INC_PUSS_LUA_MODULE_H_
#define _INC_PUSS_LUA_MODULE_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

typedef struct _LuaProxy	LuaProxy;

typedef struct PussInterface {
	LuaProxy*	(*luaproxy)			(void);
	void*		(*require)			(lua_State* L, const char* m, void* ud);
	const char*	(*app_path)			(lua_State* L);
	int			(*rawget_ex)		(lua_State* L, const char* name);
	int			(*pcall_stacktrace)	(lua_State* L, int n, int r);
	void        (*push_const_table) (lua_State* L);
} PussInterface;

#ifdef _WIN32
	#define PUSS_MODULE_EXPORT __declspec(dllexport)
#else
	#define PUSS_MODULE_EXPORT	extern __attribute__ ((visibility("default"))) 
#endif

// puss module need :
//   extern "C"
//   PUSS_MODULE_EXPORT
//   void* __puss_module_init__(lua_State* L, PussInterface* puss, void* ud);
// 
// return module interface
// 
typedef void* (*PussModuleInit)(lua_State* L, PussInterface* puss, void* ud);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_MODULE_H_

