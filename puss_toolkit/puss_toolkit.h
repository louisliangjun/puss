// puss_toolket.h

#ifndef _INC_PUSS_TOOLKET_H_
#define _INC_PUSS_TOOLKET_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

// puss module

#define PUSS_KEY_PUSS			"_@Puss@_"
#define PUSS_KEY_NEWSTATE		"_@PussNewState@_"

typedef void (*PussSetup)(lua_State* L, const char* app_path, const char* app_name, const char* app_config);

void	puss_lua_setup_base(lua_State* L, const char* app_path, const char* app_name, const char* app_config);
void	puss_lua_parse_arg0(lua_State* L, const char* arg0, PussSetup setup);

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
