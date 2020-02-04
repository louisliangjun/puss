// cobject_demo.c

#include "puss_plugin.h"

#include "cobject_demo.h"

// cd /d d:\github\puss && tinycc\win32\tcc -shared -Iinclude -o bin\plugins\cobject_demo.dll bin\test\cobject_demo.c


static int formular_demo_object_f(const PussCStackObject* stobj, lua_Integer field, PussCValue* nv) {
	DemoObject* obj = demo_object_check(stobj);
	nv->i = obj->b + obj->e;
	return TRUE;
}

static int formular_demo_object_n(const PussCStackObject* stobj, lua_Integer field, PussCValue* nv) {
	DemoObject* obj = demo_object_check(stobj);
	nv->i = obj->l * obj->m;
	return TRUE;
}

static void on_changed(const PussCStackObject* stobj, lua_Integer field) {
	lua_State* L = stobj->L;
	lua_getglobal(L, "print");
	lua_pushstring(L, "cobject-on-changed");
	lua_pushvalue(L, 1);
	lua_pushinteger(L, field);
	lua_call(L, 3, 0);
}

static int demo_module_reg(lua_State* L) {
	// push plugin module as ref
	if( lua_isnoneornil(L, 2) )
		luaL_argerror(L, 2, "formular ref needed");
	lua_settop(L, 2);

	puss_cschema_changed_reset(L, 1, "DemoModule", (PussCObjectChanged)on_changed);
	
#define demo_formular_reg(prop) puss_cschema_formular_reset(L, 1, demo_object_field(prop), formular_demo_object_ ## prop)
	demo_formular_reg(f);
	demo_formular_reg(n);
#undef demo_formular_reg

	return 0;
}

static int demo_module_test(lua_State* L) {
	const DemoObject* obj = demo_object_checkudata(L, 1);
	PussCStackObject stobj = { puss_cobject_cast(obj), L, 1 };
	lua_Integer n = luaL_optinteger(L, 2, 10);
	lua_Integer i;

#if 1
	for( i=0; i<n; ++i ) {
		demo_object_set(&stobj, a, obj->a + i);
		demo_object_set(&stobj, b, obj->b + i);
		demo_object_set(&stobj, c, obj->c + i);
		demo_object_set(&stobj, d, obj->d + i);
		demo_object_set(&stobj, e, obj->e + i);
		demo_object_set(&stobj, f, obj->f + i);
		demo_object_set(&stobj, g, obj->g + i);
		demo_object_set(&stobj, h, obj->h + i);
	}
#else
	for( i=0; i<n; ++i ) {
		lua_pushinteger(L, obj->a + i);	puss_cobject_stack_set( &stobj, demo_object_field(a) );
		lua_pushinteger(L, obj->b + i);	puss_cobject_stack_set( &stobj, demo_object_field(b) );
		lua_pushinteger(L, obj->c + i);	puss_cobject_stack_set( &stobj, demo_object_field(c) );
		lua_pushinteger(L, obj->d + i);	puss_cobject_stack_set( &stobj, demo_object_field(d) );
		lua_pushinteger(L, obj->e + i);	puss_cobject_stack_set( &stobj, demo_object_field(e) );
		lua_pushinteger(L, obj->f + i);	puss_cobject_stack_set( &stobj, demo_object_field(f) );
		lua_pushinteger(L, obj->g + i);	puss_cobject_stack_set( &stobj, demo_object_field(g) );
		lua_pushinteger(L, obj->h + i);	puss_cobject_stack_set( &stobj, demo_object_field(h) );
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

