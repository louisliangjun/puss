// puss_gobject_ffi_reg.h

#include "puss_gobject_module.h"

// GType register methods utils

#define out		GFFI_PESUDO_TYPE_OUT_ARG, 
#define refout	GFFI_PESUDO_TYPE_REF_OUT_ARG, 
#define inout	GFFI_PESUDO_TYPE_INOUT_ARG, 
#define opt		GFFI_PESUDO_TYPE_OPT_ARG, 

// register function format : see PussGObjectReg

#define REG_SYMBOLS_INDEX 1
#define REG_CONSTS_INDEX  2

#define gtype_reg_global_ffi(rtype, func, ...) \
	reg_iface->push_gfunction(L, #func, rtype, func, __VA_ARGS__, G_TYPE_INVALID); \
	lua_setfield(L, REG_SYMBOLS_INDEX, #func)

#define gtype_reg_global_ffi_rnew(rtype, func, ...) \
	reg_iface->push_gfunction(L, "*" #func, rtype, func, __VA_ARGS__, G_TYPE_INVALID); \
	lua_setfield(L, REG_SYMBOLS_INDEX, #func)

#define gtype_reg_start(gtype, prefix) \
	{ \
		const size_t __gtype_reg_prefix_len = sizeof(#prefix); \
		(void)__gtype_reg_prefix_len;  \
		reg_iface->push_gtype_index_table(L, gtype, #prefix)

#define gtype_reg_end() \
		lua_pop(L, 1); \
	}

#define _gtype_reg_setfield(func) do {\
		lua_pushvalue(L, -1); \
		lua_setfield(L, -3, ((const char*)#func) + __gtype_reg_prefix_len); \
		lua_setfield(L, REG_SYMBOLS_INDEX, (const char*)#func); \
	} while(0)

#define gtype_reg_ffi_full(rtype, prefix, name, func, ...) do {\
		reg_iface->push_gfunction(L, prefix name, rtype, func, __VA_ARGS__, G_TYPE_INVALID); \
		lua_pushvalue(L, -1); \
		lua_setfield(L, -3, ((const char*)name) + __gtype_reg_prefix_len); \
		lua_setfield(L, REG_SYMBOLS_INDEX, (const char*)name); \
	} while (0)

#define gtype_reg_lua(func) \
	lua_pushcfunction(L, _lua_##func); \
	_gtype_reg_setfield(func)

#define gtype_reg_ffi(rtype, func, ...) \
	reg_iface->push_gfunction(L, #func, rtype, func, __VA_ARGS__, G_TYPE_INVALID); \
	_gtype_reg_setfield(func)

#define gtype_reg_ffi_rename(rtype, func, ...) \
	reg_iface->push_gfunction(L, #func, rtype, _rename_##func, __VA_ARGS__, G_TYPE_INVALID); \
	_gtype_reg_setfield(func)

#define gtype_reg_ffi_rnew(rtype, func, ...) \
	reg_iface->push_gfunction(L, "*" #func, rtype, func, __VA_ARGS__, G_TYPE_INVALID); \
	_gtype_reg_setfield(func)

#define gtype_reg_boxed_new_use_c_struct0(gtype, func, ctype) \
	reg_iface->push_c_struct0_boxed_type_new_method(L, gtype, sizeof(ctype)); \
	_gtype_reg_setfield(func)

