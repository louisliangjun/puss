// vlua.c
// 
// build: gcc -s -O2 -pthread -Iinclude -Ipuss_toolkit -o tools/vlua tools/vlua.c puss_toolkit/*.c -llua -lm -ldl

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "puss_lua.h"
#include "puss_toolkit.h"

static int			vlua_argc = 0;
static const char*	vlua_argv[256];

// args

static int lua_fetch_args(lua_State* L) {
	int i;
	lua_createtable(L, vlua_argc, 0);
	for( i=0; i<vlua_argc; ++i ) {
		lua_pushstring(L, vlua_argv[i]);
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static int lua_match_arg(lua_State* L) {
	int nret = 0;
	int top, i;
	if( puss_get_value(L, "string.match")!=LUA_TFUNCTION )
		return luaL_error(L, "string.match missing");

	top = lua_gettop(L);
	for( i=0; i<vlua_argc; ++i ) {
		lua_pushvalue(L, top);
		lua_pushstring(L, vlua_argv[i]);
		lua_pushvalue(L, 1);
		lua_call(L, 2, LUA_MULTRET);
		if( !lua_isnoneornil(L, top+1) ) {
			nret = lua_gettop(L) - top;
			break;
		}
		lua_settop(L, top);
	}
	return nret;
}

// files

static int lua_file_stat(lua_State* L) {
	const char* filename = luaL_checkstring(L, 1);
	struct stat st;
	if( stat(filename, &st) )
		return 0;
	lua_pushboolean(L, 1);
	lua_pushinteger(L, (lua_Integer)st.st_size);
	lua_pushinteger(L, (lua_Integer)st.st_mtime);
	return 3;
}

static int lua_file_copy(lua_State* L) {
	size_t slen = 0;
	const char* src = luaL_checklstring(L, 1, &slen);
	size_t dlen = 0;
	const char* dst = luaL_checklstring(L, 2, &dlen);
	#ifdef _WIN32
		lua_pushboolean(L, CopyFileA(src, dst, 1));
	#else
		luaL_Buffer B;
		luaL_buffinitsize(L, &B, slen + dlen + 32);
		sprintf(B.b, "cp %s %s", src, dst);
		lua_pushboolean(L, system(B.b)==0);
	#endif
	return 1;
}

static int lua_file_mkdir(lua_State* L) {
	const char* dirpath = luaL_checkstring(L, 1);
	#ifdef _WIN32
		lua_pushboolean(L, CreateDirectoryA(dirpath, NULL));
	#else
		int mode = (int)luaL_optinteger(L, 2, 0777);
		lua_pushboolean(L, mkdir(dirpath, mode)==0);
	#endif
	return 1;
}

// vlua lib for build
// 
static const luaL_Reg vlua_methods[] =
	{ {"fetch_args", lua_fetch_args}
	, {"match_arg", lua_match_arg}
	, {"file_stat", lua_file_stat}
	, {"file_copy", lua_file_copy}
	, {"file_mkdir", lua_file_mkdir}
	, {NULL, NULL}
	};

static int vlua_reg(lua_State* L) {
	luaL_setfuncs(L, vlua_methods, 0);
	return 0;
}

static int show_help(void) {
	printf("usage : vlua <filename> [args]\n");
	printf("usage : vlua -e <script> [args]\n");
	return 0;
}

static const char* vlua_args_parse(int* is_script_file, int argc, char** argv) {
	int script_arg = 0;
	int is_exec = 0;
	int i;

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
		vlua_argv[vlua_argc++] = argv[i];
	}

	if( is_exec ) {
		*is_script_file = 0;
		return argv[script_arg + 1];
	}

	*is_script_file = 1;
	return argv[script_arg];
}

const char* vlua_main = "-- vlua_main.lua\n"
	"local is_script_file, script = ...\n"
	"local main_trunk, err\n"
	"if is_script_file then\n"
	"	main_trunk, err = loadfile(script, 'bt')\n"
	"else\n"
	"	main_trunk, err = load(script, '<-e>', 'bt')\n"
	"end\n"
	"if not main_trunk then error('load error: '..tostring(err)) end\n"
	"puss._main = string.dump(main_trunk)\n"
	"main_trunk()\n"
	"if is_script_file and main then main() end\n"
	;

int main(int argc, char** argv) {
	int is_script_file = 0;
	const char* script = vlua_args_parse(&is_script_file, argc, argv);
	lua_State* L;
	int res = 0;
	luaL_Reg extras[] =
		{ {"vlua", vlua_reg}
		, {NULL, NULL}
		};

	if( !script )
		return show_help();

	puss_config_init(argc, argv);
	__puss_config__.extra_builtin_reg = extras;

	L = puss_lua_newstate();
	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	if( luaL_loadbuffer(L, vlua_main, strlen(vlua_main), "<vlua>") ) {
		fprintf(stderr, "load main error: %s", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}
	lua_pushboolean(L, is_script_file);
	lua_pushstring(L, script);
	if( lua_pcall(L, 2, 0, 1) ) {
		fprintf(stderr, "vlua run error: %s", lua_tostring(L, -1));
		lua_close(L);
		return 2;
	}
	lua_close(L);
	return res;
}

