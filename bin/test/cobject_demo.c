// cobject_demo.c

#include "puss_plugin.h"

#include "cobject_demo.h"

// cd /d d:\github\puss && tinycc\win32\tcc -shared -Iinclude -o bin\plugins\cobject_demo.dll bin\test\cobject_demo.c

static int formular_demo_object_f(lua_State* L, const DemoObject* obj, lua_Integer field) {
	lua_pushinteger(L, obj->b + obj->e);
	return 1;
}

static int formular_demo_object_n(lua_State* L, const DemoObject* obj, lua_Integer field) {
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

static void _demo_formular_reg(lua_State* L, int creator, lua_Integer field, int (*formular)(lua_State* L, const DemoObject* obj, lua_Integer field)) {
	puss_cschema_formular_reset(L, creator, field, (PussCObjectFormula)formular);
}

static int demo_module_reg(lua_State* L) {
	// push plugin module as ref
	if( lua_isnoneornil(L, 2) )
		luaL_argerror(L, 2, "formular ref needed");
	lua_settop(L, 2);

	puss_cschema_changed_reset(L, 1, "DemoModule", (PussCObjectChanged)on_changed);

#define demo_formular_reg(prop) _demo_formular_reg(L, 1, demo_object_field(prop), formular_demo_object_ ## prop)
	demo_formular_reg(f);
	demo_formular_reg(n);
#undef demo_formular_reg

	return 0;
}

static int demo_module_test(lua_State* L) {
	const DemoObject* obj = demo_object_check(L, 1);
	lua_Integer n = luaL_optinteger(L, 2, 10);
	lua_Integer i;

#if 1
	for( i=0; i<n; ++i ) {
		demo_object_set(L, obj, a, obj->a + i);
		demo_object_set(L, obj, b, obj->b + i);
		demo_object_set(L, obj, c, obj->c + i);
		demo_object_set(L, obj, d, obj->d + i);
		demo_object_set(L, obj, e, obj->e + i);
		demo_object_set(L, obj, f, obj->f + i);
		demo_object_set(L, obj, g, obj->g + i);
		demo_object_set(L, obj, h, obj->h + i);
	}
#else
	const PussCObject* cobj = (const PussCObject*)obj;
	for( i=0; i<n; ++i ) {
		lua_pushinteger(L, obj->a + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_A);
		lua_pushinteger(L, obj->b + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_B);
		lua_pushinteger(L, obj->c + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_C);
		lua_pushinteger(L, obj->d + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_D);
		lua_pushinteger(L, obj->e + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_E);
		lua_pushinteger(L, obj->f + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_F);
		lua_pushinteger(L, obj->g + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_G);
		lua_pushinteger(L, obj->h + i);	puss_cobject_stack_set(L, cobj, DEMO_OBJECT_H);
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

