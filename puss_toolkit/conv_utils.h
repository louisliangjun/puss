// conv_utils.h

#ifndef _INC_PUSSTOOLKIT_CONV_UTILS_H_
#define _INC_PUSSTOOLKIT_CONV_UTILS_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

size_t	puss_format_filename(char* fname);

int		puss_lua_filename_format(lua_State* L);
int		puss_lua_file_list(lua_State* L);
int		puss_lua_local_to_utf8(lua_State* L);
int		puss_lua_utf8_to_local(lua_State* L);

PUSS_DECLS_END

#endif//_INC_PUSSTOOLKIT_CONV_UTILS_H_
