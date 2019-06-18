#include "../header.h"

static int foo(lua_State* L) {
	lua_Integer a = luaL_checkinteger(L, 1);
	lua_Integer b = luaL_checkinteger(L, 2);
	lua_Integer c = luaL_optinteger(L, 3, 1);
	lua_Integer r = (a + b) / c;
	lua_pushinteger(L, r);
	return 1;
}

static int bar(lua_State* L) {
	lua_pushinteger(L, 1);
	lua_pushstring(L, "abc");
	return 2;
}

int __module_init(lua_State* L) {
	lua_newtable(L);
	lua_pushcfunction(L, foo);
	lua_setfield(L, -2, "foo");
	lua_pushcfunction(L, bar);
	lua_setfield(L, -2, "bar");
	return 1;
}
