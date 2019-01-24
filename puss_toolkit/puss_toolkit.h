// puss_toolket.h

#ifndef _INC_PUSS_TOOLKET_H_
#define _INC_PUSS_TOOLKET_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

// puss toolkit

typedef enum _PussToolkitKey
	{ PUSS_KEY_NULL
	, PUSS_KEY_PUSS			// puss {table}
	, PUSS_KEY_THREAD_ENV	// ThreadEnv
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

int		puss_lua_setup_base(lua_State* L);

#define	PUSS_LUA_GET(L, K)	((*(__puss_toolkit_sink__.state_get_key))((L), (K)))

// export C utils

size_t	puss_format_filename(char* fname);
void*	puss_simple_pack(size_t* plen, lua_State* L, int start, int end);
int		puss_simple_unpack(lua_State* L, const void* pkt, size_t len);

// export Lua utils

void	puss_conv_utils_reg(lua_State* L);
void	puss_simple_pickle_reg(lua_State* L);
void	puss_async_service_reg(lua_State* L);
void	puss_thread_service_reg(lua_State* L);

PUSS_DECLS_END

#endif//_INC_PUSS_TOOLKET_H_
