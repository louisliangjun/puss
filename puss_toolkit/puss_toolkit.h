// puss_toolket.h

#ifndef _INC_PUSS_TOOLKET_H_
#define _INC_PUSS_TOOLKET_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

// puss toolkit

typedef enum _PussToolkitKey
	{ PUSS_KEY_INVALID = 0
	, PUSS_KEY_PUSS
	, PUSS_KEY_CONST_TABLE
	, PUSS_KEY_ERROR_HANDLE
	} PussToolkitKey;

typedef int  (*PussKeyGet)(lua_State* L, PussToolkitKey key);
typedef void (*PussKeySet)(lua_State* L, PussToolkitKey key);
typedef lua_State* (*PussStateNew)(void);

typedef struct _PussConfig {
	int				app_argc;
	char**			app_argv;

	const char*		app_path;
	const char*		app_name;
	const char*		app_config;

	lua_Alloc		app_alloc_fun;
	void*			app_alloc_tag;

	PussStateNew	state_new;
	PussKeyGet		state_get_key;
	PussKeySet		state_set_key;

	lua_CFunction	plugin_loader_reg;
	luaL_Reg*		extra_builtin_reg;
} PussConfig;

extern PussConfig	__puss_config__;

void		puss_config_init(int argc, char* argv[]);

#define		puss_lua_get(L, K)	((*(__puss_config__.state_get_key))((L), (K)))
#define		puss_lua_newstate()	((*(__puss_config__.state_new))())

// export C utils

size_t		puss_format_filename(char* fname);
int			puss_get_value(lua_State* L, const char* path);
void*		puss_simple_pack(size_t* plen, lua_State* L, int start, int end);
int			puss_simple_unpack(lua_State* L, const void* pkt, size_t len);

// puss builtin modules declare

int			puss_toolkit_open(lua_State* L);

int			puss_reg_puss_utils(lua_State* L);
int			puss_reg_simple_pickle(lua_State* L);
int			puss_reg_simple_luastate(lua_State* L);
int			puss_reg_async_service(lua_State* L);
int			puss_reg_thread_service(lua_State* L);

PUSS_DECLS_END

#endif//_INC_PUSS_TOOLKET_H_
