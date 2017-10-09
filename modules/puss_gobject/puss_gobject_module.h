// puss_gobject_module.h

#ifndef _INC_PUSS_GOBJECT_MODULE_H_
#define _INC_PUSS_GOBJECT_MODULE_H_

#include "puss_module.h"

#include <stdlib.h>
#include <stdarg.h>
#include <glib.h>
#include <glib-object.h>

PUSS_DECLS_BEGIN

#define GFFI_PESUDO_TYPE_OUT_ARG		G_TYPE_MAKE_FUNDAMENTAL(G_TYPE_RESERVED_USER_FIRST + 1)
#define GFFI_PESUDO_TYPE_REF_OUT_ARG	G_TYPE_MAKE_FUNDAMENTAL(G_TYPE_RESERVED_USER_FIRST + 2)
#define GFFI_PESUDO_TYPE_INOUT_ARG		G_TYPE_MAKE_FUNDAMENTAL(G_TYPE_RESERVED_USER_FIRST + 3)
#define GFFI_PESUDO_TYPE_OPT_ARG		G_TYPE_MAKE_FUNDAMENTAL(G_TYPE_RESERVED_USER_FIRST + 4)

// create lua C function & push to L
// atypes :
//	1 ends with G_TYPE_INVALID
//	2 can use GFFI_PESUDO_TYPE_OUT_ARG for out argument
//	3 can use GFFI_PESUDO_TYPE_REF_OUT_ARG for out argument, ref(out value)
//	4 can use GFFI_PESUDO_TYPE_INOUT_ARG for in-out argument
//	5 can use GFFI_PESUDO_TYPE_OPT_ARG for opt input argument(can be null)
//	6 if use pesudo type, MUST use it before real GType.
//	7 name[0]=='*' means return value need free
// 
// for example : void add(gint* result, gint a, gint b);
//		iface->push_gfunction(L, "add", G_TYPE_NONE, add, GFFI_PESUDO_TYPE_OUT_ARG, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID)
// 
// if return value need free, name[0] used for mark it: gchar* foo(void) { return g_strdup("some str"); }
//		iface->push_gfunction(L, "*foo", G_TYPE_STRING, foo);
// 
typedef struct PussGObjectRegIface	PussGObjectRegIface;
typedef void	(*PussGObjectReg)	(lua_State* L, PussGObjectRegIface* reg_iface, int glua_env_index);

struct PussGObjectRegIface {
	gboolean	(*push_gfunction)			(lua_State* L, const char* name, GType rtype, const void* addr, ... /*GType atype ... */);
	gboolean	(*push_gtype_index_table)	(lua_State* L, GType type, const char* prefix);	// [-0,+1,-]
	void		(*push_c_struct0_boxed_type_new_method)	(lua_State* L, GType type, gsize struct_size);	// [-0,+1,-]

	void		(*reg_gtype)				(lua_State* L, int glua_env_index, GType type, const char* prefix, const luaL_Reg* methods);
	void		(*reg_genum)				(lua_State* L, int glua_env_index, GType type);
};

typedef struct PussGObjectInterface {
	// gobject
	void		(*push_master_table)		(lua_State* L);	// [-0,+1,-]

	void		(*module_reg)				(lua_State* L, PussGObjectReg f);

	GValue*		(*gvalue_check)				(lua_State* L, int idx);
	GValue*		(*gvalue_test)				(lua_State* L, int idx);
	GValue*		(*gvalue_check_type)		(lua_State* L, int idx, GType type);
	GValue*		(*gvalue_new)				(lua_State* L, GType init_type);
	void		(*gvalue_push)				(lua_State* L, const GValue* v);
	GValue*		(*gvalue_push_boxed)		(lua_State* L, GType tp, gconstpointer ptr, gboolean take);	// take==FALSE means need copy
	void		(*gvalue_from_lua)			(lua_State* L, int idx, GValue* v);

	void		(*gobject_push)				(lua_State* L, GObject* obj);	// [-0,+1,e]
	GObject*	(*gobject_check)			(lua_State* L, int idx);
	GObject*	(*gobject_test)				(lua_State* L, int idx);
	gpointer	(*gobject_check_type)		(lua_State* L, int idx, GType type);

	int			(*gsignal_connect)			(lua_State* L);	// [-3|4,+1,e]	connect(GObject* obj, const char* detailed_signal, function, gboolean after)

} PussGObjectInterface;

PUSS_DECLS_END

#endif//_INC_PUSS_GOBJECT_MODULE_H_

