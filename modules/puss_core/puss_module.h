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
	#define puss_app_path				(*(__puss_iface__->app_path))
	#define puss_rawget_ex				(*(__puss_iface__->rawget_ex))
	#define puss_pcall_stacktrace		(*(__puss_iface__->pcall_stacktrace))
	#define puss_debug_command			(*(__puss_iface__->debug_command))

	#define	__lua_proxy__(sym)			(*(__puss_iface__->luaproxy.sym))
#endif

PUSS_DECLS_END

#include "luaproxy.h"

PUSS_DECLS_BEGIN

typedef enum PussDebugCmd
	{ PUSS_DEBUG_CMD_RESET		// p=PussDebugEventHandle, n=count hook
	, PUSS_DEBUG_CMD_BP_SET		// p=filename, n=line, return 0 if set failed
	, PUSS_DEBUG_CMD_BP_DEL		// p=filename, n=line
	, PUSS_DEBUG_CMD_STEP_INTO
	, PUSS_DEBUG_CMD_STEP_OVER
	, PUSS_DEBUG_CMD_STEP_OUT
	, PUSS_DEBUG_CMD_CONTINUE
	, PUSS_DEBUG_CMD_RUNTO		// p=filename or NULL, n=line
	} PussDebugCmd;

typedef enum PussDebugEvent
	{ PUSS_DEBUG_EVENT_ATTACHED
	, PUSS_DEBUG_EVENT_HOOK_COUNT
	, PUSS_DEBUG_EVENT_BREAKED_BEGIN
	, PUSS_DEBUG_EVENT_BREAKED_UPDATE
	, PUSS_DEBUG_EVENT_BREAKED_END
	} PussDebugEvent;

typedef void   (*PussDebugEventHandle)	(lua_State* L, enum PussDebugEvent ev);

struct PussInterface {
	// module
	void		(*module_require)		(lua_State* L, const char* name);	// [-0,+0,e]
	void		(*interface_register)	(lua_State* L, const char* name, void* iface);	// [-0,+0,e]
	void*		(*interface_check)		(lua_State* L, const char* name);	// [-0,+0,e]

	// consts
	void        (*push_const_table)		(lua_State* L);	// [-0,+1,-]

	// misc
	const char*	(*app_path)				(lua_State* L);	// [-0,+0,-]
	int			(*rawget_ex)			(lua_State* L, const char* name);	// [-0,+1,-]
	int			(*pcall_stacktrace)		(lua_State* L, int n, int r);		// [-0,+1,-]

	// debug
	int			(*debug_command)		(lua_State* L, PussDebugCmd cmd, const void* p, int n);

	// luaproxy
	LuaProxy	luaproxy;
};

#ifdef __cplusplus
	#define PUSS_EXTERN_C	extern "C"
#else
	#define PUSS_EXTERN_C
#endif

#ifdef _WIN32
	#define PUSS_MODULE_EXPORT	PUSS_EXTERN_C __declspec(dllexport)
#else
	#define PUSS_MODULE_EXPORT	PUSS_EXTERN_C extern __attribute__ ((visibility("default"))) 
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

