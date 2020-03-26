// main.c

#ifdef _MSC_VER
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
	#include <windows.h>
	#include <winternl.h>
	#include <conio.h>
	#include <io.h>
#else
	#include <setjmp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define _PUSS_IMPLEMENT
#include "puss_plugin.h"
#include "puss_toolkit.h"

#define _PUSS_PLUGIN_BASIC_PROTECT
#ifdef _WIN32
	#define _PUSS_PLUGIN_USE_MEMORY_PE
#endif
#include "puss_debug.inl"
#include "puss_plugin.inl"

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

static const char* puss_push_parse_args(lua_State* L, int* is_script_file, int argc, char** argv) {
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

#ifdef _WIN32
	static int puss_error_handle_win(lua_State* L) {
		luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 0);
		if( lua_istable(L, -1) && lua_getfield(L, -1, "traceback")==LUA_TFUNCTION ) {
			lua_pushvalue(L, 1);
			lua_call(L, 1, 1);
		} else {
			lua_settop(L, 1);
		}
		MessageBoxA(NULL, lua_tostring(L, -1), "PussError", MB_OK|MB_ICONERROR);
		lua_settop(L, 1);
		return 1;
	}
#endif

static const char* _push_script_filename(lua_State* L, const char* script) {
	return script ? lua_pushstring(L, script) : lua_pushfstring(L, "%s/" PUSS_DEFAULT_SCRIPT_FILE, __puss_config__.app_path);
}

static int puss_init(lua_State* L) {
	int is_script_file = 0;
	const char* script = NULL;

	puss_lua_get(L, PUSS_KEY_PUSS);
	script = puss_push_parse_args(L, &is_script_file, __puss_config__.app_argc, __puss_config__.app_argv);
	lua_setfield(L, -2, "_args");			// puss._args
	lua_pushboolean(L, is_script_file);
	lua_setfield(L, -2, "_is_script_file");	// puss._is_script_file
	script = is_script_file ? _push_script_filename(L, script) : lua_pushstring(L, script);
	lua_setfield(L, -2, "_script");			// puss._script

	if( is_script_file ) {
		lua_getfield(L, -1, "dofile");
		lua_getfield(L, -2, "_script");
		lua_call(L, 1, 0);
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

int main(int argc, char* argv[]) {
	lua_State* L = NULL;
	int reboot_as_debug_level = 0;
	const char* debug_level = puss_args_lookup(argc, argv, "--debug");
#ifdef _WIN32
	const char* console_mode = puss_args_lookup(argc, argv, "--console");
#endif
	puss_config_init(argc, argv);
	__puss_config_debug_level__ = (debug_level==NULL) ? 0 : (*debug_level=='\0' ? 1 : (int)strtol(debug_level, NULL, 10));
	__puss_config__.state_new = puss_lua_debugger_newstate;
	__puss_config__.plugin_loader_reg = puss_plugin_loader_reg;

restart_label:
#ifdef _WIN32
	if( console_mode || (reboot_as_debug_level)) {
		if( !GetConsoleWindow() ) {
			HANDLE hConIn, hConOut;
			AllocConsole();
			// crt
			freopen("CONIN$", "r+t", stdin);
			freopen("CONOUT$", "w+t", stdout);
			freopen("CONOUT$", "w+t", stderr);
			// posix
			_dup2(_fileno(stdin), 0);
			_dup2(_fileno(stdout), 1);
			_dup2(_fileno(stderr), 2);
			// win32
			hConOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			hConIn = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
			SetStdHandle(STD_ERROR_HANDLE, hConOut);
			SetStdHandle(STD_INPUT_HANDLE, hConIn);
		}
	}
#endif
	reboot_as_debug_level = (__puss_config_debug_level__==0);

	L = puss_lua_newstate();
	lua_settop(L, 0);

	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
#ifdef _WIN32
	if( !console_mode ) {
		lua_pop(L, 1);
		lua_pushcfunction(L, puss_error_handle_win);
	}
#endif

	lua_pushcfunction(L, puss_init);
	if( lua_pcall(L, 0, 1, 1) )	// init
		return 1;
	if( lua_pcall(L, 0, 1, 1) )	// main
		return 2;
	if( reboot_as_debug_level ) {
		puss_lua_get(L, PUSS_KEY_PUSS);
		lua_getfield(L, -1, "_reboot_as_debug_level");
		__puss_config_debug_level__ = (int)lua_tointeger(L, -1);
		lua_pop(L, 2);
		reboot_as_debug_level = __puss_config_debug_level__ ? 1 : 0;
	}
	lua_close(L);

	if( reboot_as_debug_level )
		goto restart_label;

	return 0;
}

