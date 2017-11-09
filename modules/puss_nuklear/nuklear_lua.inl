// nuklear_lua.inl

// usage :
/*
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "nuklear.h"
#include "nuklear_lua.inl"
*/

//----------------------------------------------------------

// apis
#define USE_LUA_NK_API_IMPLEMENT
	#include "nuklear_lua_apis.inl"
#undef USE_LUA_NK_API_IMPLEMENT

// types
static int nk_char_array_new_lua(lua_State* L) {
	struct nk_char_array* v;
	size_t m = 0;
	size_t n = 0;
	const char* s = NULL;
	if( lua_type(L,1)==LUA_TSTRING ) {
		s = luaL_checklstring(L, 1, &n);
		m = (size_t)luaL_optinteger(L, 2, (lua_Integer)n);
	} else {
		m = (size_t)luaL_checkinteger(L, 1);
	}
	v = (struct nk_char_array*)_lua_new_nk_boxed(L, "nk_char_array", sizeof(struct nk_char_array) + sizeof(char)*m);
	v->max = (int)m;
	v->len = (int)((n<m) ? n : m);
	if( (v->len > v->max) || (v->len < 0) || (v->max < 0) )
		return luaL_argerror(L, 1, "size error");
	if( s ) { memcpy(v->arr, s, v->len); }
	return 1;
}

static int nk_string_array_new_lua(lua_State* L) {
	struct nk_string_array* v;
	int i, n;
	luaL_checktype(L, 1, LUA_TTABLE);

	n = (int)luaL_len(L, 1);
	v = (struct nk_string_array*)_lua_new_nk_boxed(L, "nk_string_array", sizeof(struct nk_string_array) + sizeof(const char*)*n);
	v->len = n;

	lua_createtable(L, n, 0);
	for( i=1; i<=n; ++i ) {
		luaL_argcheck(L, lua_geti(L, 1, i)==LUA_TSTRING, 1, "array element need string");
		v->arr[i-1] = lua_tostring(L, -1);
		lua_rawseti(L, -2, i);
	}
	lua_setuservalue(L, -2);
	return 1;
}

static int nk_float_array_new_lua(lua_State* L) {
	struct nk_float_array* v;
	int i, n;
	luaL_checktype(L, 1, LUA_TTABLE);

	n = (int)luaL_len(L, 1);
	v = (struct nk_float_array*)_lua_new_nk_boxed(L, "nk_float_array", sizeof(struct nk_float_array) + sizeof(float)*n);
	v->len = n;

	for( i=1; i<=n; ++i ) {
		luaL_argcheck(L, lua_geti(L, 1, i)==LUA_TNUMBER, 1, "array element need number");
		v->arr[i-1] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return 1;
}

static int nk_vec2_array_new_lua(lua_State* L) {
	struct nk_vec2_array* v;
	int i, n;
	luaL_checktype(L, 1, LUA_TTABLE);

	n = (int)luaL_len(L, 1) / 2;
	v = (struct nk_vec2_array*)_lua_new_nk_boxed(L, "nk_vec2_array", sizeof(struct nk_vec2_array) + sizeof(struct nk_vec2)*n);
	v->len = n;

	for( i=0; i<n; ++i ) {
		luaL_argcheck(L, lua_geti(L, 1, i*2+1)==LUA_TNUMBER, 1, "array element need number");
		v->arr[i].x = lua_tonumber(L, -1);
		lua_pop(L, 1);
		luaL_argcheck(L, lua_geti(L, 1, i*2+2)==LUA_TNUMBER, 1, "array element need number");
		v->arr[i].y = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return 1;
}

static int nk_buffer_free_lua(lua_State* L) {
	nk_buffer_free( lua_check_nk_buffer(L, 1) );
	return 0;
}

static int nk_buffer_new_lua(lua_State* L) {
	struct nk_buffer* ud = (struct nk_buffer*)lua_newuserdata(L, sizeof(struct nk_buffer));
	nk_buffer_init_default(ud);
	if( luaL_newmetatable(L, "nk_buffer") ) {
		lua_pushcfunction(L, nk_buffer_free_lua);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static int nk_textedit_free_lua(lua_State* L) {
	nk_textedit_free( lua_check_nk_text_edit(L, 1) );
	return 0;
}

static int nk_textedit_new_lua(lua_State* L) {
	struct nk_text_edit* ud = (struct nk_text_edit*)lua_newuserdata(L, sizeof(struct nk_text_edit));
	nk_textedit_init_default(ud);
	if( luaL_newmetatable(L, "nk_text_edit") ) {
		lua_pushcfunction(L, nk_textedit_free_lua);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return 1;
}

// --------------------------------------------------
// extern apis for lua wrapper

static int nk_vec2_xy_lua(lua_State* L) {
	struct nk_vec2* v = lua_check_nk_vec2(L,1);
	lua_pushnumber(L, v->x), lua_pushnumber(L, v->y);
	return 2;
}

static luaL_Reg nuklera_lua_apis[] =
	{ {"nk_char_array_new", nk_char_array_new_lua}
	, {"nk_string_array_new", nk_string_array_new_lua}
	, {"nk_float_array_new", nk_float_array_new_lua}
	, {"nk_vec2_array_new", nk_vec2_array_new_lua}
	
	, {"nk_buffer_new", nk_buffer_new_lua}
	, {"nk_textedit_new", nk_textedit_new_lua}
	, {"nk_vec2_xy", nk_vec2_xy_lua}
	
#define USE_LUA_NK_API_REGISTER
	#include "nuklear_lua_apis.inl"
#undef USE_LUA_NK_API_REGISTER

	, { NULL, NULL }
	};

static int lua_new_nk_lib(lua_State* L) {
	if( lua_getfield(L, LUA_REGISTRYINDEX, "puss_nulklear_lib")==LUA_TTABLE )
		return 0;

	lua_pop(L, 1);

	lua_newtable(L);
	luaL_setfuncs(L, nuklera_lua_apis, 0);

#define _lua_nk_reg_filter(filter)	lua_push_nk_plugin_filter(L, filter); lua_setfield(L, -2, #filter)
	_lua_nk_reg_filter(nk_filter_default);
	_lua_nk_reg_filter(nk_filter_ascii);
	_lua_nk_reg_filter(nk_filter_float);
	_lua_nk_reg_filter(nk_filter_decimal);
	_lua_nk_reg_filter(nk_filter_hex);
	_lua_nk_reg_filter(nk_filter_oct);
	_lua_nk_reg_filter(nk_filter_binary);
#undef _lua_nk_reg_filter
	return 1;
}

