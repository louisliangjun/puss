// puss_module_glfw.c

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define _GLFWPROXY_NOT_USE_SYMBOL_MACROS
#include "puss_module_glfw.h"

static PussGLFWInterface glfw_iface;

static void register_enums(lua_State* L) {
	puss_push_const_table(L);

	#define __GLFWPROXY_ENUM(sym)		lua_pushinteger(L, sym); lua_setfield(L, -2, #sym);
	#include "GLFW/glfw3proxy.enums"
	#undef __GLFWPROXY_ENUM

	lua_pop(L, 1);
}

static luaL_Reg glfw_methods[] =
	{ {"", NULL}
	, {"", NULL}
	, {"", NULL}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
		__puss_iface__ = puss;

		#define __GLFWPROXY_SYMBOL(sym)		glfw_iface.sym = sym;
		#include "GLFW/glfw3proxy.symbols"
		#undef __GLFWPROXY_SYMBOL
	}

	puss_interface_register(L, "PussGLFWInterface", &glfw_iface); // register C interface
	register_enums(L);

	luaL_newlib(L, glfw_methods);
	return 0;
}

