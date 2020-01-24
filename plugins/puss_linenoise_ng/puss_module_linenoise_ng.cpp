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

static int lua_linenoiseReadLine(lua_State* L) {
	const char* prompt = luaL_optstring(L, 1, "");
	int setup = (linenoiseCompletionState==NULL) && lua_isfunction(L, lua_upvalueindex(1));
	char* result;
#ifdef _WIN32
	if (!GetConsoleWindow())	return 0;
#endif
	if( setup ) {
		linenoiseCompletionState = L;
		linenoiseSetCompletionCallback(linenoiseCompletionCallbackWrapper);
	}
	result = linenoise(prompt);
	if( setup )	{
		linenoiseSetCompletionCallback(NULL);
		linenoiseCompletionState = NULL;
	}
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
#ifdef _WIN32
	if (!GetConsoleWindow())	return 0;
#endif
	linenoiseClearScreen();
	return 0;
}

static int lua_linenoiseSetMultiLine(lua_State* L) {
	linenoiseSetMultiLine(lua_toboolean(L, 1));
	return 0;
}

static int lua_linenoisePrintKeyCodes(lua_State* L) {
#ifdef _WIN32
	if (!GetConsoleWindow())	return 0;
#endif
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
	{ {"AddCompletion", lua_linenoiseAddCompletion}
	, {"PreloadBuffer", lua_linenoisePreloadBuffer}
	, {"HistoryAdd", lua_linenoiseHistoryAdd}
	, {"HistorySetMaxLen", lua_linenoiseHistorySetMaxLen}
	, {"HistoryLine", lua_linenoiseHistoryLine}
	, {"HistorySave", lua_linenoiseHistorySave}
	, {"HistoryLoad", lua_linenoiseHistoryLoad}
	, {"HistoryFree", lua_linenoiseHistoryFree}
	, {"ClearScreen", lua_linenoiseClearScreen}
	, {"SetMultiLine", lua_linenoiseSetMultiLine}
	, {"PrintKeyCodes", lua_linenoisePrintKeyCodes}
	, {"InstallWindowChangeHandler", lua_linenoiseInstallWindowChangeHandler}
	, {"KeyType", lua_linenoiseKeyType}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
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
	lua_pushcclosure(L, lua_linenoiseReadLine, 1);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ReadLine");
	lua_pushcclosure(L, lua_linenoiseSetCompletionCallback, 1);
	lua_setfield(L, -2, "SetCompletionCallback");
	return 1;
}

