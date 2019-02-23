// simple_luastate.c

#include "puss_toolkit.h"

#include <stdlib.h>
#include <string.h>

#define PUSS_KEY_SIMPLELUASTATE		"_@PussSimpleLuaState@_"

typedef struct _SimpleLuaState {
	lua_State*	L;
} SimpleLuaState;

static int simple_luastate_close(lua_State* L) {
	SimpleLuaState* ud = (SimpleLuaState*)luaL_checkudata(L, 1, PUSS_KEY_SIMPLELUASTATE);
	if( ud->L ) {
		lua_close(ud->L);
		ud->L = NULL;
	}
	return 0;
}

static int _luastate_pcall(lua_State* L) {
	lua_State* from = (lua_State*)lua_touserdata(L, 1);
	size_t len = 0;
	void* pkt = puss_simple_pack(&len, from, 2, -1);
	lua_settop(L, 0);
	puss_simple_unpack(L, pkt, len);

	if( lua_type(L, 1)==LUA_TSTRING ) {
		puss_get_value(L, lua_tostring(L, 1));
	} else {
		lua_pushvalue(L, 1);
		lua_pushglobaltable(L);
		lua_replace(L, 1);
		lua_gettable(L, 1);
	}
	lua_replace(L, 1);
	lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
	return lua_gettop(L);
}

static int _luastate_reply(lua_State* L) {
	lua_State* from = (lua_State*)lua_touserdata(L, 1);
	int top = (int)lua_tointeger(L, 2);
	size_t len = 0;
	void* pkt = puss_simple_pack(&len, from, top, -1);
	lua_settop(L, 0);
	puss_simple_unpack(L, pkt, len);
	return lua_gettop(L);
}

static int simple_luastate_pcall(lua_State* L) {
	SimpleLuaState* ud = (SimpleLuaState*)luaL_checkudata(L, 1, PUSS_KEY_SIMPLELUASTATE);
	lua_State* state = ud->L;
	int top = lua_gettop(L);
	int res;
	if( !state ) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "lua state already free!");
		return 2;
	}
	if( top < 2 ) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "no enought args!");
		return 2;
	}

	top = lua_gettop(state);
	puss_lua_get(state, PUSS_KEY_ERROR_HANDLE);
	lua_pushcfunction(state, _luastate_pcall);
	lua_pushlightuserdata(state, L);
	res = lua_pcall(state, 1, LUA_MULTRET, top+1);
	lua_settop(L, 0);
	lua_pushboolean(L, res==LUA_OK);
	lua_pushcfunction(L, _luastate_reply);
	lua_pushlightuserdata(L, state);
	lua_pushinteger(L, top+1);
	res = lua_pcall(L, 2, LUA_MULTRET, 0);
	lua_settop(state, top);
	if( res ) {
		lua_pushboolean(L, 0);
		lua_replace(L, 1);
	}
	return lua_gettop(L);
}

static luaL_Reg simple_luastate_methods[] =
	{ {"__gc", simple_luastate_close}
	, {"close", simple_luastate_close}
	, {"pcall", simple_luastate_pcall}
	, {NULL, NULL}
	};

static int simple_luastate_new(lua_State* L) {
	SimpleLuaState* ud = (SimpleLuaState*)lua_newuserdata(L, sizeof(SimpleLuaState));
	memset(ud, 0, sizeof(SimpleLuaState));
	if( luaL_newmetatable(L, PUSS_KEY_SIMPLELUASTATE) ) {
		luaL_setfuncs(L, simple_luastate_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	ud->L = puss_lua_newstate();
	return 1;
}

int puss_reg_simple_luastate(lua_State* L) {
	lua_pushcfunction(L, simple_luastate_new);
	lua_setfield(L, -2, "simple_luastate_new");	// puss.simple_luastate_new
	return 0;
}
