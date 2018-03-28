// puss_module_nuklear.h

#ifndef _INC_PUSS_MODULE_NUKLEAR_GLFW3_H_
#define _INC_PUSS_MODULE_NUKLEAR_GLFW3_H_

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "nuklear.h"

#include "puss_module.h"

#ifndef _PUSS_NUKLEAR_MODULE_IMPLEMENT
	#define	__nuklear_proxy__(sym)		(*(__puss_nuklear_iface__->nuklear_proxy.sym))
#else
	#define _NUKLEARPROXY_NOT_USE_SYMBOL_MACROS
#endif

#include "nuklear_proxy.h"

PUSS_DECLS_BEGIN

typedef struct PussNuklearInterface {
	struct nk_context*	(*check_nk_context)(lua_State* L, int arg);
	struct nk_font*		(*check_nk_font)(lua_State* L, int arg);

	void				(*clipbard_set_string)(struct nk_context* ctx, const char* text);
	const char*			(*clipbard_get_string)(struct nk_context* ctx);

	NuklearProxy		nuklear_proxy;
} PussNuklearInterface;

#ifndef _PUSS_NUKLEAR_MODULE_IMPLEMENT
	extern PussNuklearInterface* __puss_nuklear_iface__;
#endif

PUSS_DECLS_END

#endif//_INC_PUSS_MODULE_NUKLEAR_GLFW3_H_

