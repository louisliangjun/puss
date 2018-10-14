// main.c

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "puss_core/puss_lua.h"

#ifndef _PUSS_MODULE_SUFFIX
	#define _PUSS_MODULE_SUFFIX	".so"
#endif

#define PUSS_DEFAULT_SCRIPT_FILE "default.lua"

static const char* puss_args_lookup(int argc, char** argv, const char* key) {
	size_t len = strlen(key);
	int i;
	for( i=1; i<argc; ++i ) {
		const char* arg = argv[i];
		if( strncmp(arg, key, len)!=0 )
			continue;
		if( arg[len]=='\0' )
			return arg+len;
		if( arg[len]=='=' )
			return arg+len+1;
	}
	return NULL;
}

static const char* puss_push_parse_args(lua_State* L, int* is_script_file, int argc, const char** argv) {
	int script_arg = 0;
	int is_exec = 0;
	lua_Integer n = 0;
	int i;

	lua_newtable(L);
	for( i=argc-1; i>0; --i ) {
		if( argv[i][0] != '-' ) {
			script_arg = i;
		} else if( strcmp(argv[i], "-e")==0 || strcmp(argv[i], "--execute")==0 ) {
			if( (i+1) < argc ) {
				script_arg = i;
				is_exec = 1;
				break;
			}
		}
	}

	for( i=1; i<argc; ++i ) {
		if( i==script_arg ) {
			if( is_exec ) {
				++i;
			}
			continue;
		}
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, ++n);
	}

	if( is_exec ) {
		*is_script_file = 0;
		return argv[script_arg + 1];
	}

	*is_script_file = 1;
	return script_arg==0 ? NULL : argv[script_arg];
}

static int puss_dummy_main(lua_State* L) {
	fprintf(stderr, "not find function __main__() in lua _G!");
	return 0;
}

static void push_error_str(lua_State* L) {
	if( lua_getglobal(L, "debug")==LUA_TTABLE && lua_getfield(L, -1, "traceback")==LUA_TFUNCTION ) {
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
	} else {
		lua_pushvalue(L, 1);
	}
}

static int puss_error_handle(lua_State* L) {
	int top = lua_gettop(L);
	push_error_str(L);
	fprintf(stderr, "[Error] %s\n", luaL_tolstring(L, -1, NULL));
	lua_settop(L, top);
	return lua_gettop(L);
}

#ifdef _WIN32
	#define is_path_sep(ch) ((ch)=='/' || (ch)=='\\')
	static int puss_error_handle_win(lua_State* L) {
		int top = lua_gettop(L);
		push_error_str(L);
		MessageBoxA(NULL, lua_tostring(L, -1), "ERROR", MB_OK|MB_ICONERROR);
		lua_settop(L, top);
		return lua_gettop(L);
	}
#else
	#define puss_error_handle_win	puss_error_handle
#endif

static const char* puss_push_script_filename(lua_State* L, const char* script) {
	if( script ) {
		lua_pushstring(L, script);
	} else {
		lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS);
		lua_getfield(L, -1, "_path");
		lua_getfield(L, -2, "_sep");
		lua_remove(L, -3);
		lua_pushstring(L, PUSS_DEFAULT_SCRIPT_FILE);
		lua_concat(L, 3);
		script = lua_tostring(L, -1);
	}
	return script;
}

static int puss_init(lua_State* L) {
	int argc = (int)lua_tointeger(L, 1);
	const char** argv = (const char**)lua_touserdata(L, 2);
	int is_script_file = 0;
	const char* script = NULL;

	lua_getglobal(L, "puss");

	script = puss_push_parse_args(L, &is_script_file, argc, argv);
	lua_setfield(L, -2, "_args");			// puss._args
	lua_pushboolean(L, is_script_file);
	lua_setfield(L, -2, "_is_script_file");	// puss._is_script_file
	script = is_script_file ? puss_push_script_filename(L, script) : lua_pushstring(L, script);
	lua_setfield(L, -2, "_script");			// puss._script

	if( is_script_file ) {
		luaL_dostring(L, "puss.dofile(puss._script)");
		if( lua_getglobal(L, "__main__")!=LUA_TFUNCTION ) {
			lua_pop(L, 1);
			lua_pushcfunction(L, puss_dummy_main);
		}
	} else if( luaL_loadbuffer(L, script, strlen(script), "<-e>") ) {
		lua_error(L);
	}
	// fprintf( stderr, "puss_init() return: %s\n", lua_typename(L, lua_type(L, -1)) );
	return 1;
}

static int puss_main(int argc, char* argv[], int debug_level, int console_mode) {
	lua_State* L = NULL;
	int res = 0;
	int reboot_as_debug_level = 0;

restart_label:
	L = puss_lua_newstate(debug_level, NULL, NULL);

#ifdef _WIN32
	if( console_mode || (reboot_as_debug_level)) {
		AllocConsole();
		freopen("CONIN$", "r+t", stdin);
		freopen("CONOUT$", "w+t", stdout);
		freopen("CONOUT$", "w+t", stderr);
	}
#endif
	reboot_as_debug_level = (debug_level==0);

	luaL_openlibs(L);
	puss_lua_open_default(L, argv[0], _PUSS_MODULE_SUFFIX);

	lua_getglobal(L, "xpcall");
	lua_pushcfunction(L, puss_init);
	lua_pushcfunction(L, console_mode ? puss_error_handle : puss_error_handle_win);
	lua_pushinteger(L, argc);
	lua_pushlightuserdata(L, argv);
	lua_call(L, 4, 2);
	res = lua_toboolean(L, -2) ? 0 : 1;
	if( res ) {
		fprintf(stderr, "puss_init() failed: %s\n", lua_tostring(L, -1));
	} else {
		lua_getglobal(L, "xpcall");
		lua_replace(L, -3);
		lua_pushcfunction(L, console_mode ? puss_error_handle : puss_error_handle_win);
		lua_call(L, 2, 1);
		res = lua_toboolean(L, -1) ? 0 : 2;
	}
	if( reboot_as_debug_level ) {
		int top = lua_gettop(L);
		debug_level = 0;
		if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PUSS)==LUA_TTABLE ) {
			lua_getfield(L, -1, "_reboot_as_debug_level");
			debug_level = (int)lua_tointeger(L, -1);
		}
		lua_settop(L, top);
		reboot_as_debug_level = debug_level ? 1 : 0;
	}
	lua_close(L);

	if( reboot_as_debug_level )
		goto restart_label;

	return res;
}

int main(int argc, char* argv[]) {
	const char* debug_level = puss_args_lookup(argc, argv, "--debug");
	const char* console_mode = puss_args_lookup(argc, argv, "--console");
	int res = puss_main(argc, argv
		, (debug_level==NULL) ? 0 : (*debug_level=='\0' ? 1 : (int)strtol(debug_level, NULL, 10))
		, console_mode ? 1 : 0);
#ifdef _WIN32
	if( console_mode ) { system("PAUSE"); }
#endif
	return res;
}

