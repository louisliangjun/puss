// puss_module.h

#ifndef _INC_PUSS_LUA_MODULE_H_
#define _INC_PUSS_LUA_MODULE_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

typedef struct PussInterface	PussInterface;

#ifdef _PUSS_MODULE_IMPLEMENT
	#define _LUAPROXY_NOT_USE_SYMBOL_MACROS
#else
	extern PussInterface* __puss_iface__;

	#define puss_module_require			(*(__puss_iface__->module_require))
	#define puss_interface_register		(*(__puss_iface__->interface_register))
	#define puss_interface_check(L, I)	((I*)((*(__puss_iface__->interface_check))((L), #I)))
	#define puss_push_const_table		(*(__puss_iface__->push_const_table))

	#define	__lua_proxy__(sym)			(*(__puss_iface__->luaproxy.sym))
#endif

PUSS_DECLS_END

#include "luaproxy.h"

PUSS_DECLS_BEGIN

struct PussInterface {
	void		(*module_require)		(lua_State* L, const char* name);				// [-0,+1,e]
	void		(*interface_register)	(lua_State* L, const char* name, void* iface);	// [-0,+0,e]
	void*		(*interface_check)		(lua_State* L, const char* name);				// [-0,+0,e]
	void        (*push_const_table)		(lua_State* L);	// [-0,+1,-]
	LuaProxy	luaproxy;
};

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
//   local module = puss.require(module_name, p1, p2, ...)
// 
// puss module implements example :
//   struct ModuleIFace {
//      void* (*foo)(int, const char*);
//   };
//   
//   static struct ModuleIFace module_iface = { foo };
//   
//   PussInterface* __puss_iface__ = NULL;
//   
//   PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
//     __puss_iface__ = puss;
//     puss_interface_register(L, "ModuleIFace", &module_iface); // register C interface
//     luaL_newlib(L, module_methods); // lua interface
//     reutrn 1;
//   }
// 
typedef int (*PussModuleInit)(lua_State* L, PussInterface* puss);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_MODULE_H_

