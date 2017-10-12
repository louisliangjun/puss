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
	void*		(*require)			(lua_State* L, const char* name);	// [-0,+0,e]
	void        (*push_const_table) (lua_State* L);	// [-0,+1,-]
	const char*	(*app_path)			(lua_State* L);	// [-0,+0,-]
	int			(*rawget_ex)		(lua_State* L, const char* name);	// [-0,+1,-]
	int			(*pcall_stacktrace)	(lua_State* L, int n, int r);		// [-0,+1,-]
} PussInterface;

#ifdef _WIN32
	#define PUSS_MODULE_EXPORT __declspec(dllexport)
#else
	#define PUSS_MODULE_EXPORT	extern __attribute__ ((visibility("default"))) 
#endif

// puss module need :
//   extern "C" PUSS_MODULE_EXPORT void* __puss_module_init__(lua_State* L, PussInterface* puss);
// 
// usage :
//   local module = puss.require(module_name, p1, p2, ...)
// 
// example :
//   struct ModuleIFace {
//      void* (*foo)(int, const char*);
//      int   (*bar)(void);
//   };
// 
//   static struct ModuleIFace module_iface = { foo, bar };
// 
//   extern "C" PUSS_MODULE_EXPORT void* __puss_module_init__(lua_State* L, PussInterface* puss) {
//      puss->luaproxy()->lua_newtable(L);	// module iface for lua
//      return &module_iface;				// module iface for C
//   }
//   
typedef void* (*PussModuleInit)(lua_State* L, PussInterface* puss);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_MODULE_H_

