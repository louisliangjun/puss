// puss_plugin.h

#ifndef _INC_PUSS_LUA_PLUGIN_H_
#define _INC_PUSS_LUA_PLUGIN_H_

#include "puss_lua.h"

typedef struct _PussInterface	PussInterface;

#ifdef _PUSS_IMPLEMENT
	#define _LUAPROXY_NOT_USE_SYMBOL_MACROS
#else
	extern PussInterface*			__puss_iface__;

	#define	__lua_proxy_sym__(sym)	(*(__puss_iface__->lua_proxy.sym))

	#define puss_interface_check(L,IFace)			(IFace*)((*(__puss_iface__->interface_check))((L), #IFace))
	#define puss_interface_register(L,name,iface)	(*(__puss_iface__->interface_register))((L),(name),(iface))
	#define puss_push_consts_table(L)				(*(__puss_iface__->push_consts_table))((L))
#endif

#include "luaproxy.h"

PUSS_DECLS_BEGIN

struct _PussInterface {
	struct LuaProxy	lua_proxy;

	void*	(*interface_check)(lua_State* L, const char* name);
	void	(*interface_register)(lua_State* L, const char* name, void* iface);
	void	(*push_consts_table)(lua_State* L);
};

#ifdef _WIN32
	#define PUSS_PLUGIN_EXPORT	PUSS_EXTERN_C __declspec(dllexport)
#else
	#define PUSS_PLUGIN_EXPORT	PUSS_EXTERN_C __attribute__ ((visibility("default"))) 
#endif

// puss plugin usage :
//   local plugin = puss.require(plugin_name)
// 
// puss plugin implements example :
//   struct SomeIFace {
//      void* (*foo)(int, const char*);
//   };
//   
//   static struct SomeIFace some_iface = { foo };
//   
//   PussInterface* __puss_iface__ = NULL;
//   
//   PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
//     __puss_iface__ = puss;
//     puss_interface_register(L, "SomeIFace", &some_iface); // register C interface
//     luaL_newlib(L, some_methods); // lua interface
//     reutrn 1;
//   }
// 
typedef int (*PussPluginInit)(lua_State* L, PussInterface* puss);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_PLUGIN_H_

