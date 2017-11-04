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
#define NK_INCLUDE_COMMAND_USERDATA

#include "nuklear.h"

#include "puss_module.h"

typedef struct PussNuklearInterface	PussNuklearInterface;

#ifdef _PUSS_NUKLEAR_MODULE_IMPLEMENT
	#define _NUKLEARPROXY_NOT_USE_SYMBOL_MACROS
#else
	extern PussNuklearInterface* __puss_nuklear_iface__;

	#define	__nuklear_proxy__(sym)		(*(__puss_nuklear_iface__->nuklear_proxy.sym))
#endif

#include "nuklear_proxy.h"

PUSS_DECLS_BEGIN

struct PussNuklearInterface {
	NuklearProxy	nuklear_proxy;
};


PUSS_DECLS_END

#endif//_INC_PUSS_MODULE_NUKLEAR_GLFW3_H_

