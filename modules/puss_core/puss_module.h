// puss_module.h

#ifndef _INC_PUSS_LUA_MODULE_H_
#define _INC_PUSS_LUA_MODULE_H_

#include "puss_macros.h"

typedef struct LuaProxy	LuaProxy;

#ifdef _PUSS_MODULE_IMPLEMENT
	#define _LUAPROXY_NOT_USE_SYMBOL_MACROS
#else
	extern struct LuaProxy*			__lua_proxy__;
	#define	__lua_proxy_sym__(sym)	(*(__lua_proxy__->sym))
	
	#define puss_interface_register(L,name,iface)	(*((PussInterface*)(__lua_proxy__->userdata))->interface_register)((L),(name),(iface))
	#define puss_interface_check(L,IFace)			((IFace*)((*((PussInterface*)(__lua_proxy__->userdata))->interface_check)((L), #IFace)
	#define puss_push_consts_table(L)				(*((PussInterface*)(__lua_proxy__->userdata))->push_consts_table)((L))
#endif

#include "luaproxy.h"

typedef struct PussInterface {
	void*	(*interface_check)(lua_State* L, const char* name);
	void	(*interface_register)(lua_State* L, const char* name, void* iface);
	void	(*push_consts_table)(lua_State* L);
} PussInterface;

#ifdef __cplusplus
	#define PUSS_EXTERN_C	extern "C"
#else
	#define PUSS_EXTERN_C	extern
#endif

#ifdef _WIN32
	#define PUSS_MODULE_EXPORT	PUSS_EXTERN_C __declspec(dllexport)
#else
	#define PUSS_MODULE_EXPORT	PUSS_EXTERN_C __attribute__ ((visibility("default"))) 
#endif

// puss module usage :
//   local module = puss.require(module_name)
// 
// puss module implements example :
//   struct ModuleIFace {
//      void* (*foo)(int, const char*);
//   };
//   
//   static struct ModuleIFace module_iface = { foo };
//   
//   LuaProxy* __lua_proxy__ = NULL;
//   
//   PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, LuaProxy* lua) {
//     __lua_proxy__ = lua;
//     puss_interface_register(L, "ModuleIFace", &module_iface); // register C interface
//     luaL_newlib(L, module_methods); // lua interface
//     reutrn 1;
//   }
// 
typedef int (*PussModuleInit)(lua_State* L, LuaProxy* lua);

#endif//_INC_PUSS_LUA_MODULE_H_

