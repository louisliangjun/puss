// puss_glfw_module.c

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "puss_module.h"

#define _GLFWPROXY_NOT_USE_SYMBOL_MACROS
#include "glfw3proxy.h"

static GLFWProxy glfwproxy;

PussInterface* __puss_iface__ = NULL;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
		__puss_iface__ = puss;

		#define __GLFWPROXY_SYMBOL(sym)		glfwproxy.sym = sym;
		#include "glfw3proxy.symbols"
		#undef __GLFWPROXY_SYMBOL
	}
	puss_interface_register(L, "ModuleIFace", &module_iface); // register C interface

	reutrn 0;
}

