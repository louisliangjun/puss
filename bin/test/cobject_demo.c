// cobject_demo.c

#include "puss_plugin.h"

#include "cobject_demo.h"

// cd /d d:\github\puss && tinycc\win32\tcc -shared -Iinclude -o bin\plugins\cobject_demo.dll bin\test\cobject_demo.c

static int formular_demo_object_F(lua_State* L, const DemoObject* obj, lua_Integer field) {
	lua_pushinteger(L, obj->b + obj->e);
	return 1;
}

static int formular_demo_object_N(lua_State* L, const DemoObject* obj, lua_Integer field) {
	lua_pushinteger(L, obj->l * obj->m);
	return 1;
}

static void on_changed(lua_State* L, const DemoObject* obj, lua_Integer field) {
	lua_getglobal(L, "print");
	lua_pushstring(L, "cobject-on-changed");
	lua_pushvalue(L, 1);
	lua_pushinteger(L, field);
	lua_call(L, 3, 0);
}

static int demo_module_reg(lua_State* L) {
	// push plugin module as ref
	lua_pushvalue(L, lua_upvalueindex(1)); 
	puss_cschema_changed_reset(L, 1, "DemoModule", on_changed);

	#define formula_reg(field)	lua_pushvalue(L, lua_upvalueindex(1)); puss_cschema_formular_reset(L, 1, DEMO_OBJECT_ ## field, (PussCObjectFormula)formular_demo_object_ ## field)
		formula_reg(F);
		formula_reg(N);
	#undef demo_object_formula_reg
	return 0;
}

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	lua_pushcfunction(L, demo_module_reg);
	return 1;
}

