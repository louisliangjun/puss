// nuklear_lua.inl

static void* _lua_new_nk_boxed(lua_State* L, const char* tname, size_t struct_sz);
static void  _lua_push_nk_boxed(lua_State* L, const char* tname, const void* src, size_t len);

#define lua_check_nk_enum(L, arg, T)		(enum T)luaL_checkinteger((L), (arg))
#define lua_check_nk_flags(L, arg)			(nk_flags)luaL_checkinteger((L), (arg))
#define lua_check_nk_int(L, arg)			(int)luaL_checkinteger((L), (arg))
#define lua_check_nk_uint(L, arg)			(nk_uint)luaL_checkinteger((L), (arg))
#define lua_check_nk_size(L, arg)			(nk_size)luaL_checkinteger((L), (arg))
#define lua_check_nk_float(L, arg)			(float)luaL_checknumber((L), (arg))
#define lua_check_nk_double(L, arg)			luaL_checknumber((L), (arg))

#define lua_new_nk_boxed(L, T)				(T*)_lua_new_nk_boxed((L), #T, sizeof(T))
#define lua_push_nk_boxed(L, ptr, T)		_lua_push_nk_boxed((L), #T, ptr, sizeof(T))
#define lua_check_nk_boxed(L, arg, T)		(T*)luaL_checkudata((L), (arg), #T)
#define lua_check_nk_enum_boxed(L, arg, T)	(enum T*)luaL_checkudata((L), (arg), #T)
#define lua_check_nk_flags_boxed(L, arg)	lua_check_nk_boxed((L), (arg), nk_flags)
#define lua_check_nk_bool_boxed(L, arg)		lua_check_nk_boxed((L), (arg), int)
#define lua_check_nk_int_boxed(L, arg)		lua_check_nk_boxed((L), (arg), nk_int)
#define lua_check_nk_uint_boxed(L, arg)		lua_check_nk_boxed((L), (arg), nk_uint)
#define lua_check_nk_size_boxed(L, arg)		lua_check_nk_boxed((L), (arg), nk_size)
#define lua_check_nk_float_boxed(L, arg)	lua_check_nk_boxed((L), (arg), float)
#define lua_check_nk_double_boxed(L, arg)	lua_check_nk_boxed((L), (arg), double)

#define lua_new_nk_struct(L, T)				(struct T*)_lua_new_nk_boxed((L), #T, sizeof(struct T))
#define lua_push_nk_struct(L, ptr, T)		_lua_push_nk_boxed((L), #T, ptr, sizeof(struct T))
#define lua_check_nk_struct(L, arg, T)		(struct T*)luaL_checkudata((L), (arg), #T)
#define lua_check_nk_vec2(L, arg)			lua_check_nk_struct((L), (arg), nk_vec2)
#define lua_check_nk_rect(L, arg)			lua_check_nk_struct((L), (arg), nk_rect)
#define lua_check_nk_color(L, arg)			lua_check_nk_struct((L), (arg), nk_color)
#define lua_check_nk_image(L, arg)			lua_check_nk_struct((L), (arg), nk_image)
#define lua_check_nk_context(L, arg)		lua_check_nk_struct((L), (arg), nk_context)
#define lua_check_nk_text_edit(L, arg)		lua_check_nk_struct((L), (arg), nk_text_edit)
#define lua_check_nk_scroll(L, arg)			lua_check_nk_struct((L), (arg), nk_scroll)
#define lua_check_nk_list_view(L, arg)		lua_check_nk_struct((L), (arg), nk_list_view)
#define lua_check_nk_cursor(L, arg)			lua_check_nk_struct((L), (arg), nk_cursor)
#define lua_check_nk_user_font(L, arg)		lua_check_nk_struct((L), (arg), nk_user_font)
#define lua_check_nk_draw_list(L, arg)		lua_check_nk_struct((L), (arg), nk_draw_list)
#define lua_check_nk_style_item(L, arg)		lua_check_nk_struct((L), (arg), nk_style_item)
#define lua_check_nk_style_button(L, arg)	lua_check_nk_struct((L), (arg), nk_style_button)
#define lua_check_nk_font_atlas(L, arg)		lua_check_nk_struct((L), (arg), nk_font_atlas)
#define lua_check_nk_font_config(L, arg)	lua_check_nk_struct((L), (arg), nk_font_config)
#define lua_check_nk_font(L, arg)			lua_check_nk_struct((L), (arg), nk_font)
#define lua_check_nk_buffer(L, arg)			lua_check_nk_struct((L), (arg), nk_buffer)
#define lua_check_nk_input(L, arg)			lua_check_nk_struct((L), (arg), nk_input)
#define lua_check_nk_command_buffer(L, arg)	lua_check_nk_struct((L), (arg), nk_command_buffer)

#define lua_check_nk_plugin_filter(L, arg)	(nk_plugin_filter)luaL_checkudata((L), (arg), "nk_plugin_filter")

static inline void lua_push_nk_vec2(lua_State* L, struct nk_vec2 v) { lua_push_nk_struct(L, &v, nk_vec2); }
static inline void lua_push_nk_rect(lua_State* L, struct nk_rect v) { lua_push_nk_struct(L, &v, nk_rect); }
static inline void lua_push_nk_color(lua_State* L, struct nk_color v) { lua_push_nk_struct(L, &v, nk_color); }
static inline void lua_push_nk_image(lua_State* L, struct nk_image v) { lua_push_nk_struct(L, &v, nk_image); }
static inline void lua_push_nk_style_item(lua_State* L, struct nk_style_item v) { lua_push_nk_struct(L, &v, nk_style_item); }

static inline void lua_push_nk_plugin_filter(lua_State* L, nk_plugin_filter v) { lua_push_nk_boxed(L, &v, nk_plugin_filter); }

static inline void lua_push_nk_window_ptr(lua_State* L, struct nk_window* v) { lua_push_nk_boxed(L, &v, struct nk_window*); }
static inline void lua_push_nk_panel_ptr(lua_State* L, struct nk_panel* v) { lua_push_nk_boxed(L, &v, struct nk_panel*); }
static inline void lua_push_nk_font_ptr(lua_State* L, struct nk_font* v) { lua_push_nk_boxed(L, &v, struct nk_font*); }

static void* _lua_new_nk_boxed(lua_State* L, const char* tname, size_t struct_sz) {
	void* ud = lua_newuserdata(L, struct_sz);
	memset(ud, 0, struct_sz);
	luaL_newmetatable(L, tname);
	lua_setmetatable(L, -2);
	return ud;
}

static void _lua_push_nk_boxed(lua_State* L, const char* tname, const void* src, size_t len) {
	void* dst = _lua_new_nk_boxed(L, tname, len);
	memcpy(dst, src, len);
}

struct nk_float_array { int len; float* arr; };

static float* lua_check_nk_float_array(lua_State* L, int arg, int len) {
	struct nk_float_array* v = lua_check_nk_boxed(L, arg, struct nk_float_array);
	luaL_argcheck(L, (len <= v->len), arg, "nk_float_array out of range");
	return v->arr;
}

struct nk_vec2_array { nk_uint len; struct nk_vec2* arr; };

static struct nk_vec2* lua_check_nk_vec2_array(lua_State* L, int arg, nk_uint len) {
	struct nk_vec2_array* v = lua_check_nk_boxed(L, arg, struct nk_vec2_array);
	luaL_argcheck(L, (len <= v->len), arg, "nk_vec2_array out of range");
	return v->arr;
}

static char lua_check_nk_char(lua_State* L, int arg) {
	char ch = 0;
	if( lua_isinteger(L, arg) ) {
		ch = (char)lua_tointeger(L, arg);
	} else {
		size_t n = 0;
		const char* s = luaL_checklstring(L, arg, &n);
		luaL_argcheck(L, n==1, 2, "need char, 1 byte string");
		ch = *s;
	}
	return ch;
}

static const char* lua_check_nk_glyph(lua_State* L, int arg) {
	size_t n = 0;
	const char* s = luaL_checklstring(L, arg, &n);
	luaL_argcheck(L, n==NK_UTF_SIZE , arg, "need glyph, 4 bytes string");
	return s;
}

static int lua_check_nk_text_len(lua_State* L, int arg) {
	size_t n = luaL_len(L, arg - 1);
	int len = (int)luaL_optinteger(L, arg, -1);
	return (len<0 || ((size_t)len>n) ) ? (int)n : len;
}

//----------------------------------------------------------

#define LUA_NK_API_NORET( api, ... ) static int api ## _lua(lua_State* L) { return api(__VA_ARGS__), 0; }
#define LUA_NK_API( push, api, ... ) static int api ## _lua(lua_State* L) { return push(L, api(__VA_ARGS__) ), 1; }
	#include "nuklear_lua_apis.inl"
#undef LUA_NK_API
#undef LUA_NK_API_NORET

static luaL_Reg nuklera_lua_apis[] = {
#define LUA_NK_API_NORET( api, ... )	{ #api, api ## _lua },
#define LUA_NK_API( push, api, ... )	{ #api, api ## _lua },
	#include "nuklear_lua_apis.inl"
#undef LUA_NK_API
#undef LUA_NK_API_NORET
	{ NULL, NULL }
	};

static void lua_open_nk_lib(lua_State* L) {
	if( lua_getfield(L, LUA_REGISTRYINDEX, "puss_nulklear_lib")==LUA_TTABLE ) {
		lua_setglobal(L, "nk");
		return;
	}
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

	lua_setglobal(L, "nk");
}

