// sample_plugin.c

#include <puss_plugin.h>

static int add(lua_State* L) {
	lua_Integer a = luaL_checkinteger(L, 1);
	lua_Integer b = luaL_checkinteger(L, 2);
	lua_pushinteger(L, a + b);
	return 1;
}

static int div(lua_State* L) {
	lua_Integer a = luaL_checkinteger(L, 1);
	lua_Integer b = luaL_checkinteger(L, 2);
	lua_pushinteger(L, a / b);	// try div zero exception
	return 1;
}

static luaL_Reg methods[] =
	{ {"add", add}
	, {"div", div}
	, { NULL, NULL }
	};

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	luaL_newlib(L, methods); // lua interface
	return 1;
}
