// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "puss_core/puss_lua.h"

#ifndef _PUSS_MODULE_SUFFIX
	#define _PUSS_MODULE_SUFFIX	".so"
#endif

#define PUSS_DEFAULT_SCRIPT_FILE "default.lua"

static int puss_get_debug(int argc, char** argv) {
	int i;
	for( i=1; i<argc; ++i ) {
		// --debug[=level]
		const char* arg = argv[i];
		if( strncmp(arg, "--debug", 7)==0 ) {
			if( arg[7]=='\0' )
				return 1;
			if( arg[7]!='=' )
				continue;
			return (int)strtol(arg+8, NULL, 10);
		}
	}
	return 0;
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

	if( script_arg==0 )
		return NULL;

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
	return argv[script_arg];
}

static int puss_dummy_main(lua_State* L) {
	fprintf(stderr, "not find function __main__() in lua _G!");
	return 0;
}

static int puss_init(lua_State* L) {
	int argc = (int)lua_tointeger(L, 1);
	const char** argv = (const char**)lua_touserdata(L, 2);
	int is_script_file = 0;
	const char* script = NULL;

	lua_getglobal(L, "puss");

	script = puss_push_parse_args(L, &is_script_file, argc, argv);
	lua_setfield(L, -2, "_args");			// puss._args
	if( !script ) {
		is_script_file = 1;
		script = PUSS_DEFAULT_SCRIPT_FILE;
	}
	lua_pushboolean(L, is_script_file);
	lua_setfield(L, -2, "_is_script_file");	// puss._is_script_file
	lua_pushstring(L, script);
	lua_setfield(L, -2, "_script");			// puss._script

	if( is_script_file ) {
		puss_rawget_ex(L, "puss.dofile");
		lua_pushstring(L, script);
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

int main(int argc, char * argv[]) {
	int res = 0;
	lua_State* L = puss_lua_newstate(puss_get_debug(argc, argv), NULL, NULL);
	luaL_openlibs(L);
	puss_lua_open_default(L, argv[0], _PUSS_MODULE_SUFFIX);

	lua_pushcfunction(L, puss_init);
	lua_pushinteger(L, argc);
	lua_pushlightuserdata(L, argv);
	if( puss_pcall_stacktrace(L, 2, 1) ) {
		fprintf(stderr, "<puss_init> error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		res = 1;
	} else if( puss_pcall_stacktrace(L, 0, 0) ) {
		fprintf(stderr, "<puss_main> error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		res = 2;
	}
	lua_close(L);
	return res;
}

