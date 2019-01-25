// puss_toolket.h

#ifndef _INC_PUSS_TOOLKET_H_
#define _INC_PUSS_TOOLKET_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

// puss toolkit

typedef enum _PussToolkitKey
	{ PUSS_KEY_NULL
	, PUSS_KEY_PUSS
	, PUSS_KEY_CONST_TABLE
	, PUSS_KEY_ERROR_HANDLE
	, PUSS_KEY_THREAD_ENV
	} PussToolkitKey;

typedef int  (*PussKeyGet)(lua_State* L, PussToolkitKey key);
typedef void (*PussKeySet)(lua_State* L, PussToolkitKey key);

typedef lua_State* (*PussStateNew)(void);

typedef struct _PussToolkitSink {
	int				app_argc;
	char**			app_argv;

	const char*		app_path;
	const char*		app_name;
	const char*		app_config;
	int				app_debug_level;

	PussStateNew	state_new;
	PussKeyGet		state_get_key;
	PussKeySet		state_set_key;
} PussToolkitSink;

extern PussToolkitSink	__puss_toolkit_sink__;

void	puss_toolkit_sink_init(int argc, char* argv[]);

int		puss_lua_init(lua_State* L);

#define	PUSS_LUA_GET(L, K)	((*(__puss_toolkit_sink__.state_get_key))((L), (K)))

// export C utils

size_t	puss_format_filename(char* fname);
int		puss_get_value(lua_State* L, const char* path);
void*	puss_simple_pack(size_t* plen, lua_State* L, int start, int end);
int		puss_simple_unpack(lua_State* L, const void* pkt, size_t len);

// puss builtin modules declare

void	puss_reg_puss_utils(lua_State* L);
void	puss_reg_simple_pickle(lua_State* L);
void	puss_reg_simple_luastate(lua_State* L);
void	puss_reg_async_service(lua_State* L);
void	puss_reg_thread_service(lua_State* L);

PUSS_DECLS_END

#endif//_INC_PUSS_TOOLKET_H_