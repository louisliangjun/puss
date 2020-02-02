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
	puss_cschema_changed_reset(L, 1, "DemoModule", (PussCObjectChanged)on_changed);

	#define formula_reg(field)	lua_pushvalue(L, lua_upvalueindex(1)); puss_cschema_formular_reset(L, 1, DEMO_OBJECT_ ## field, (PussCObjectFormula)formular_demo_object_ ## field)
		formula_reg(F);
		formula_reg(N);
	#undef demo_object_formula_reg
	return 0;
}

static int demo_module_test(lua_State* L) {
	const DemoObject* obj = DemoObjectCheck(L, 1);
	const PussCObject* cobj = (const PussCObject*)obj;
	lua_Integer n = luaL_optinteger(L, 2, 10);
	lua_Integer i;

#if 1
	for( i=0; i<n; ++i ) {
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_A, obj->a + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_B, obj->b + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_C, obj->c + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_D, obj->d + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_E, obj->e + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_F, obj->f + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_G, obj->g + i);
		puss_cobject_set_int(L, cobj, DEMO_OBJECT_H, obj->h + i);
	}
#else
	for( i=0; i<n; ++i ) {
		lua_pushinteger(L, obj->a + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_A);
		lua_pushinteger(L, obj->b + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_B);
		lua_pushinteger(L, obj->c + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_C);
		lua_pushinteger(L, obj->d + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_D);
		lua_pushinteger(L, obj->e + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_E);
		lua_pushinteger(L, obj->f + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_F);
		lua_pushinteger(L, obj->g + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_G);
		lua_pushinteger(L, obj->h + i);	puss_cobject_set(L, cobj, DEMO_OBJECT_H);
	}
#endif
	return 0;
}

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	lua_newtable(L);
	lua_pushcfunction(L, demo_module_reg);	lua_setfield(L, -2, "reg");
	lua_pushcfunction(L, demo_module_test);	lua_setfield(L, -2, "test");
	return 1;
}

