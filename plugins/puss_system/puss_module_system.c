// puss_module_system.c

#include "puss_plugin.h"

#define PUSS_SYSTEM_LIB_NAME	"[PussSystemLib]"

void puss_socket_reg(lua_State* L);
void puss_debugapi_reg(lua_State* L);

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_SYSTEM_LIB_NAME)==LUA_TTABLE ) {
		return 1;
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_SYSTEM_LIB_NAME);

	puss_socket_reg(L);
	puss_debugapi_reg(L);
	return 1;
}

