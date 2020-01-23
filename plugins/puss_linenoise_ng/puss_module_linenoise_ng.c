// puss_module_linenoise_ng.c

#include "puss_plugin.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "linenoise.h"

#define PUSS_LINENOISE_NG_LIB_NAME	"[PussLinenoiseNGLib]"

static lua_State* linenoiseCompletionState = NULL;
static linenoiseCompletions* linenoiseCompletionContent = NULL;

static void linenoiseCompletionCallbackWrapper(const char* s, linenoiseCompletions* lc) {
	lua_State* L = linenoiseCompletionState;
	if( L && lua_isfunction(L, lua_upvalueindex(1)) ) {
		int top = lua_gettop(L);
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_pushstring(L, s ? s : "");
		linenoiseCompletionContent = lc;
		lua_pcall(L, 1, 0, 0);
		linenoiseCompletionContent = NULL;
		lua_settop(L, top);
	}
}

static int lua_linenoiseAddCompletion(lua_State* L) {
	const char* s = luaL_checkstring(L, 1);
	if( linenoiseCompletionState==L && linenoiseCompletionContent ) {
		linenoiseAddCompletion(linenoiseCompletionContent, s);
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

static int lua_linenoiseSetCompletionCallback(lua_State* L) {
	lua_settop(L, 1);
	lua_setupvalue(L, lua_upvalueindex(1), 1);	// set (up[1] linenoise. up[1] cb) = cb
	return 0;
}

static int lua_linenoise(lua_State* L) {
	void linenoiseAddCompletion(linenoiseCompletions* lc, const char* str);
	const char* prompt = luaL_optstring(L, 1, "");
	int setup = (linenoiseCompletionState==NULL) && lua_isfunction(L, lua_upvalueindex(1));
	char* result;
	if( setup )	linenoiseCompletionState = L;
	result = linenoise(prompt);
	if( setup )	linenoiseCompletionState = NULL;
	if( result ) {
		lua_pushstring(L, result);
		free(result);
		return 1;
	}
	return 0;
}

static int lua_linenoisePreloadBuffer(lua_State* L) {
	linenoisePreloadBuffer(luaL_checkstring(L, 1));
	return 0;
}

static int lua_linenoiseHistoryAdd(lua_State* L) {
	lua_pushboolean(L, linenoiseHistoryAdd(luaL_checkstring(L, 1)));
	return 1;
}

static int lua_linenoiseHistorySetMaxLen(lua_State* L) {
	lua_pushboolean(L, linenoiseHistorySetMaxLen((int)luaL_checkinteger(L, 1)));
	return 1;
}

static int lua_linenoiseHistoryLine(lua_State* L) {
	char* result = linenoiseHistoryLine((int)luaL_checkinteger(L, 1));
	if( result ) {
		lua_pushstring(L, result);
		free(result);
		return 1;
	}
	return 0;
}

static int lua_linenoiseHistorySave(lua_State* L) {
	lua_pushboolean(L, linenoiseHistorySave(luaL_checkstring(L, 1)));
	return 1;
}

static int lua_linenoiseHistoryLoad(lua_State* L) {
	lua_pushboolean(L, linenoiseHistoryLoad(luaL_checkstring(L, 1)));
	return 1;
}

static int lua_linenoiseHistoryFree(lua_State* L) {
	linenoiseHistoryFree();
	return 0;
}

static int lua_linenoiseClearScreen(lua_State* L) {
	linenoiseClearScreen();
	return 0;
}

static int lua_linenoiseSetMultiLine(lua_State* L) {
	linenoiseSetMultiLine(lua_toboolean(L, 1));
	return 0;
}

static int lua_linenoisePrintKeyCodes(lua_State* L) {
	linenoisePrintKeyCodes();
	return 0;
}

static int lua_linenoiseInstallWindowChangeHandler(lua_State* L) {
	lua_pushboolean(L, linenoiseInstallWindowChangeHandler());
	return 1;
}

static int lua_linenoiseKeyType(lua_State* L) {
	lua_pushinteger(L, linenoiseKeyType());
	return 1;
}

static luaL_Reg lib_methods[] =
	{ {"linenoiseAddCompletion", lua_linenoiseAddCompletion}
	, {"linenoisePreloadBuffer", lua_linenoisePreloadBuffer}
	, {"linenoiseHistoryAdd", lua_linenoiseHistoryAdd}
	, {"linenoiseHistorySetMaxLen", lua_linenoiseHistorySetMaxLen}
	, {"linenoiseHistoryLine", lua_linenoiseHistoryLine}
	, {"linenoiseHistorySave", lua_linenoiseHistorySave}
	, {"linenoiseHistoryLoad", lua_linenoiseHistoryLoad}
	, {"linenoiseHistoryFree", lua_linenoiseHistoryFree}
	, {"linenoiseClearScreen", lua_linenoiseClearScreen}
	, {"linenoiseSetMultiLine", lua_linenoiseSetMultiLine}
	, {"linenoisePrintKeyCodes", lua_linenoisePrintKeyCodes}
	, {"linenoiseInstallWindowChangeHandler", lua_linenoiseInstallWindowChangeHandler}
	, {"linenoiseKeyType", lua_linenoiseKeyType}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
#ifdef _WIN32
	HWND hWnd = GetConsoleWindow();
	if (!hWnd) {
		AllocConsole();
		if( (hWnd = GetConsoleWindow())!=NULL ) {
			ShowWindow(hWnd, SW_SHOW);
			freopen("CONIN$", "r+t", stdin);
			freopen("CONOUT$", "w+t", stdout);
			freopen("CONOUT$", "w+t", stderr);
		}
	}
#endif
	__puss_iface__ = puss;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_LINENOISE_NG_LIB_NAME)==LUA_TTABLE ) {
		return 1;
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_LINENOISE_NG_LIB_NAME);

	luaL_setfuncs(L, lib_methods, 0);

	lua_pushboolean(L, 0);
	lua_pushcclosure(L, lua_linenoise, 1);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "linenoise");
	lua_pushcclosure(L, lua_linenoiseSetCompletionCallback, 1);
	lua_setfield(L, -2, "linenoiseSetCompletionCallback");
	return 1;
}

