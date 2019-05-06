// puss_module_tcc.c

#include "puss_plugin.h"

#ifdef _WIN32
	#include <windows.h>
#else
#endif
#include <stdlib.h>
#include <string.h>

#include "tinycc/libtcc/libtcc.h"

#define PUSS_TCC_LIB_NAME	"[PussTccLib]"
#define PUSS_TCC_NAME		"[PussTcc]"

typedef struct _ErrMsg	ErrMsg;

struct _ErrMsg {
	ErrMsg*	next;
	size_t	len;
	char	buf[1];
};

typedef struct _PussTccLua {
	TCCState*	s;
	int			relocate;
	ErrMsg*		errors;
	ErrMsg*		_elast;
} PussTccLua;

static void puss_tcc_error(void *opaque, const char *msg) {
	PussTccLua* ud = (PussTccLua*)opaque;
	size_t n = strlen(msg);
	ErrMsg* m = malloc(sizeof(ErrMsg) + n);
	if( !m )
		return;
	m->next = ud->_elast;
	m->len = n;
	memcpy(m->buf, msg, n+1);
	m->next = ud->_elast;
	ud->_elast = m;
	if( !(ud->errors) )
		ud->errors = m;
}

static int puss_tcc_destroy(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	ErrMsg* e; 
		if( ud->s ) {
		tcc_delete(ud->s);
		ud->s = NULL;
	}
	ud->_elast = NULL;
	while( (e = ud->errors)!=NULL ) {
		ud->errors = e->next;
		free(e);
	}
	return 0;
}

static int puss_tcc_set_options(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* options = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_set_options(ud->s, options);
	}
	return 0;
}

static int puss_tcc_add_include_path(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_include_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_add_sysinclude_path(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_sysinclude_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_define_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	const char* value = luaL_optstring(L, 3, NULL);
	if( ud->s ) {
		tcc_define_symbol(ud->s, sym, value);
	}
	return 0;
}

static int puss_tcc_undefine_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_undefine_symbol(ud->s, sym);
	}
	return 0;
}

static int puss_tcc_add_file(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* filename = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_file(ud->s, filename);
	}
	return 0;
}

static int puss_tcc_compile_string(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* str = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_compile_string(ud->s, str);
	}
	return 0;
}

static int puss_tcc_add_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	const void* val = lua_topointer(L, 3);
	if( ud->s ) {
		tcc_add_symbol(ud->s, name, val);
	}
	return 0;
}

#ifdef _WIN32
static const char* except_filter(unsigned long e) {
	switch( e ) {
	case EXCEPTION_ACCESS_VIOLATION:	return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:	return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_STACK_OVERFLOW:		return "EXCEPTION_STACK_OVERFLOW";
	default:	break;
	}
	return NULL;
}

static int wrap_cfunction(lua_State* L) {
	lua_CFunction f = lua_tocfunction(L, lua_upvalueindex(1));
	const char* err = NULL;
	int res;
	__try {
		res = f(L);
	}
	__except( ((err = except_filter(GetExceptionCode()))==NULL) ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) {
		luaL_error(L, err);
	}
	return res;
}
#else
static int wrap_cfunction(lua_State* L) {
	lua_CFunction f = lua_tocfunction(L, lua_upvalueindex(1));
	int res = 0;
	res = f(L);
	return res;
}
#endif

static void puss_tcc_add_c(TCCState* s) {
#define _ADDSYM(sym)	tcc_add_symbol(s, #sym, sym);
	_ADDSYM(memcmp);
	_ADDSYM(memmove);
	_ADDSYM(memcpy);
	_ADDSYM(memset);

	_ADDSYM(strcpy);
	_ADDSYM(strcat);
	_ADDSYM(strcmp);
	_ADDSYM(strlen);
	_ADDSYM(strtok);

	_ADDSYM(strtol);
	_ADDSYM(strtoul);
	_ADDSYM(strtof);
	_ADDSYM(strtod);

	_ADDSYM(sprintf);
#undef _ADDSYM
}

void puss_tcc_add_lua(TCCState* s) {
#define _ADDSYM(sym)	tcc_add_symbol(s, #sym, sym);
	_ADDSYM(lua_absindex)
	_ADDSYM(lua_gettop)
	_ADDSYM(lua_settop)
	_ADDSYM(lua_pushvalue)
	_ADDSYM(lua_rotate)
	_ADDSYM(lua_copy)
	_ADDSYM(lua_checkstack)
	_ADDSYM(lua_xmove)
	_ADDSYM(lua_isnumber)
	_ADDSYM(lua_isstring)
	_ADDSYM(lua_iscfunction)
	_ADDSYM(lua_isinteger)
	_ADDSYM(lua_isuserdata)
	_ADDSYM(lua_type)
	_ADDSYM(lua_typename)
	_ADDSYM(lua_tonumberx)
	_ADDSYM(lua_tointegerx)
	_ADDSYM(lua_toboolean)
	_ADDSYM(lua_tolstring)
	_ADDSYM(lua_rawlen)
	_ADDSYM(lua_tocfunction)
	_ADDSYM(lua_touserdata)
	_ADDSYM(lua_tothread)
	_ADDSYM(lua_topointer)
	_ADDSYM(lua_arith)
	_ADDSYM(lua_rawequal)
	_ADDSYM(lua_compare)
	_ADDSYM(lua_pushnil)
	_ADDSYM(lua_pushnumber)
	_ADDSYM(lua_pushinteger)
	_ADDSYM(lua_pushlstring)
	_ADDSYM(lua_pushstring)
	_ADDSYM(lua_pushvfstring)
	_ADDSYM(lua_pushfstring)
	_ADDSYM(lua_pushcclosure)
	_ADDSYM(lua_pushboolean)
	_ADDSYM(lua_pushlightuserdata)
	_ADDSYM(lua_pushthread)
	_ADDSYM(lua_getglobal)
	_ADDSYM(lua_gettable)
	_ADDSYM(lua_getfield)
	_ADDSYM(lua_geti)
	_ADDSYM(lua_rawget)
	_ADDSYM(lua_rawgeti)
	_ADDSYM(lua_rawgetp)
	_ADDSYM(lua_createtable)
	_ADDSYM(lua_newuserdata)
	_ADDSYM(lua_getmetatable)
	_ADDSYM(lua_getuservalue)
	_ADDSYM(lua_setglobal)
	_ADDSYM(lua_settable)
	_ADDSYM(lua_setfield)
	_ADDSYM(lua_seti)
	_ADDSYM(lua_rawset)
	_ADDSYM(lua_rawseti)
	_ADDSYM(lua_rawsetp)
	_ADDSYM(lua_setmetatable)
	_ADDSYM(lua_setuservalue)
	_ADDSYM(lua_callk)
	_ADDSYM(lua_pcallk)
	_ADDSYM(lua_yieldk)
	_ADDSYM(lua_resume)
	_ADDSYM(lua_status)
	_ADDSYM(lua_isyieldable)
	_ADDSYM(lua_error)
	_ADDSYM(lua_next)
	_ADDSYM(lua_concat)
	_ADDSYM(lua_len)
	_ADDSYM(lua_stringtonumber)
	_ADDSYM(lua_getupvalue)
	_ADDSYM(lua_setupvalue)
	_ADDSYM(lua_upvalueid)
	_ADDSYM(lua_upvaluejoin)
	_ADDSYM(luaL_getmetafield)
	_ADDSYM(luaL_callmeta)
	_ADDSYM(luaL_tolstring)
	_ADDSYM(luaL_argerror)
	_ADDSYM(luaL_checklstring)
	_ADDSYM(luaL_optlstring)
	_ADDSYM(luaL_checknumber)
	_ADDSYM(luaL_optnumber)
	_ADDSYM(luaL_checkinteger)
	_ADDSYM(luaL_optinteger)
	_ADDSYM(luaL_checkstack)
	_ADDSYM(luaL_checktype)
	_ADDSYM(luaL_checkany)
	_ADDSYM(luaL_newmetatable)
	_ADDSYM(luaL_setmetatable)
	_ADDSYM(luaL_testudata)
	_ADDSYM(luaL_checkudata)
	_ADDSYM(luaL_where)
	_ADDSYM(luaL_error)
	_ADDSYM(luaL_checkoption)
	_ADDSYM(luaL_ref)
	_ADDSYM(luaL_unref)
	_ADDSYM(luaL_loadbufferx)
	_ADDSYM(luaL_loadstring)
	_ADDSYM(luaL_len)
	_ADDSYM(luaL_gsub)
	_ADDSYM(luaL_setfuncs)
	_ADDSYM(luaL_getsubtable)
	_ADDSYM(luaL_traceback)
#undef _ADDSYM
}

static int puss_tcc_get_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	lua_CFunction func = NULL;
	if( !(ud->relocate) ) {
		ud->relocate = 1;
		puss_tcc_add_c(ud->s);
		puss_tcc_add_lua(ud->s);
		if( tcc_relocate(ud->s, TCC_RELOCATE_AUTO) != 0 ) {
			tcc_delete(ud->s);
			ud->s = NULL;
			puss_tcc_error(ud, "tcc_relocate failed!");
		}
	}
	func = (lua_CFunction)(ud->s ? tcc_get_symbol(ud->s, name) : NULL);
	if( func ) {
		lua_pushcfunction(L, func);
		lua_pushvalue(L, 1);
		lua_pushcclosure(L, wrap_cfunction, 2);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int puss_tcc_pop_error(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	ErrMsg* e = ud->errors;
	if( !e )
		return 0;
	ud->errors = e->next;
	if( !(ud->errors) )
		ud->_elast = NULL;
	lua_pushlstring(L, e->buf, e->len);
	return 1;
}

static luaL_Reg puss_tcc_methods[] =
	{ {"__gc", puss_tcc_destroy}
	, {"set_options", puss_tcc_set_options}
	, {"add_include_path", puss_tcc_add_include_path}
	, {"add_sysinclude_path", puss_tcc_add_sysinclude_path}
	, {"define_symbol", puss_tcc_define_symbol}
	, {"undefine_symbol", puss_tcc_undefine_symbol}
	, {"add_file", puss_tcc_add_file}
	, {"compile_string", puss_tcc_compile_string}
	, {"add_symbol", puss_tcc_add_symbol}
	, {"get_symbol", puss_tcc_get_symbol}
	, {"pop_error", puss_tcc_pop_error}
	, {NULL, NULL}
	};

static int puss_tcc_new(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)lua_newuserdata(L, sizeof(PussTccLua));
	ud->s = NULL;
	ud->relocate = 0;
	ud->errors = NULL;
	ud->_elast = NULL;

	if( luaL_newmetatable(L, PUSS_TCC_NAME) ) {
		luaL_setfuncs(L, puss_tcc_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	ud->s = tcc_new();
	tcc_set_error_func(ud->s, ud, puss_tcc_error);
	tcc_set_output_type(ud->s, TCC_OUTPUT_MEMORY);
	return 1;
}

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_TCC_LIB_NAME)==LUA_TTABLE ) {
		return 1;
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_TCC_LIB_NAME);

	lua_pushcfunction(L, puss_tcc_new);
	lua_setfield(L, -2, "new");
	return 1;
}

