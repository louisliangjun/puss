// puss_lua.h

#ifndef _INC_PUSS_LUA_H_
#define _INC_PUSS_LUA_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define PUSS_KEY(name)			"[" #name "]"

#define PUSS_KEY_PUSS			PUSS_KEY(puss)

// puss newstate
//  debug=0 mean NOT support debug
//  f==NULL, means use default lua_Alloc
// 
lua_State* puss_lua_newstate(int debug, lua_Alloc f, void* ud);

// puss_lua_open(L, ".", "puss", ".so")
// 
void puss_lua_open(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix);

// puss_lua_open_default(L, argv[0], ".so")
// 
void puss_lua_open_default(lua_State* L, const char* arg0, const char* module_suffix);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_H_

