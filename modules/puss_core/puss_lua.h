// puss_lua.h

#ifndef _INC_PUSS_LUA_H_
#define _INC_PUSS_LUA_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

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

// for example : puss_get_value(L, "system.test.foo")
// 
int  puss_get_value(lua_State* L, const char* name);

void* puss_pickle_pack(size_t* plen, lua_State* L, int start, int end);
int puss_pickle_unpack(lua_State* L, const void* pkt, size_t len);

PUSS_DECLS_END

#endif//_INC_PUSS_LUA_H_

