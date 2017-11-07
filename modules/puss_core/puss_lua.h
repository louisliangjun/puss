// puss_lua.h

#ifndef _INC_PUSS_LUA_H_
#define _INC_PUSS_LUA_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

void puss_lua_open(lua_State* L);

void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix);

int  puss_pcall_stacktrace(lua_State* L, int n, int r);

// for example : puss_rawget_ex(L, "system.test.foo")
// 
int  puss_rawget_ex(lua_State* L, const char* name);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_H_

