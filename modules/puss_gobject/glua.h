// glua.h

#ifndef __PUSS_INC_GOBJECT_LUA_H__
#define __PUSS_INC_GOBJECT_LUA_H__

#include "puss_gobject_module.h"

PUSS_DECLS_BEGIN

void		glua_push_master_table(lua_State* L);	// [-0,+1,-]
void		glua_push_symbol_table(lua_State* L);	// [-0,+1,-]

gboolean	glua_push_gtype_index_table(lua_State* L, GType type, const char* prefix);	// [-0,+1,-], return already exist
void		glua_push_c_struct0_boxed_type_new_method(lua_State* L, GType type, gsize struct_size);	// [-0,+1,-]

void		glua_reg_gtype(lua_State* L, int glua_symbols_index, GType type, const char* prefix, const luaL_Reg* methods);
void		glua_reg_genum(lua_State* L, int glua_consts_index, GType type);

GValue*		glua_value_check(lua_State* L, int idx);
GValue*		glua_value_test(lua_State* L, int idx);
GValue*		glua_value_check_type(lua_State* L, int idx, GType type);
GValue*		glua_value_new(lua_State* L, GType init_type);
void		glua_value_push(lua_State* L, const GValue* v);
GValue*		glua_value_push_boxed(lua_State* L, GType tp, gconstpointer ptr, gboolean take);	// take==FALSE means need copy
void		glua_value_from_lua(lua_State* L, int idx, GValue* v);

void		glua_object_push(lua_State* L, GObject* obj);	// [-0,+1,e]
GObject*	glua_object_check(lua_State* L, int idx);
GObject*	glua_object_test(lua_State* L, int idx);

gpointer	glua_object_check_type(lua_State* L, int idx, GType type);

int			glua_signal_connect(lua_State* L);	// [-3|4,+1,e]	connect(GObject* obj, const char* detailed_signal, function, gboolean after)

PUSS_DECLS_END

#endif//__PUSS_INC_GOBJECT_LUA_H__

