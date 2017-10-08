// gffi.h
//

#ifndef __PUSS_INC_GOBJECT_LUA_FFI_H__
#define __PUSS_INC_GOBJECT_LUA_FFI_H__

#include "glua.h"

PUSS_DECLS_BEGIN

void	gffi_init(void);

// void	gtype_ffi_type_reg(GType tp);

// for example : void add(gint* result, gint a, gint b);
//		GType atypes[] = { GFFI_PESUDO_TYPE_OUT_ARG, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID };
//		gffi_function_create(L, "add", G_TYPE_NONE, add, atypes);
// 
gboolean	gffi_function_create(lua_State* L, const char* name, GType rtype, const void* addr, GType* atypes);

gboolean	gffi_function_va_create(lua_State* L, const char* name, GType rtype, const void* addr, ... /*GType atype ... */ );

PUSS_DECLS_END

#endif//__PUSS_INC_GOBJECT_LUA_FFI_H__

