// nuklear_lua.inl

// TODO : now simple implements

static struct nk_context* lua_check_nk_context(lua_State* L, int arg) {
	struct nk_context* ctx = lua_touserdata(L, arg);
	luaL_argcheck(L, ctx, arg, "nil nk_context");
	return ctx;
}

static float lua_check_vec_array(lua_State* L, int arg, int n) {
	float v;
	luaL_argcheck(L, lua_rawgeti(L, arg, n)==LUA_TNUMBER, arg, "need vec array");
	v = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return v;
}

static struct nk_vec2 lua_check_nk_vec2(lua_State* L, int arg) {
	struct nk_vec2 v;
	luaL_checktype(L, arg, LUA_TTABLE);
	v.x = lua_check_vec_array(L, arg, 1);
	v.y = lua_check_vec_array(L, arg, 2);
	return v;
}

static int lua_push_nk_vec2(lua_State* L, struct nk_vec2 v) {
	lua_createtable(L, 2, 0);
	lua_pushnumber(L, v.x);	lua_rawseti(L, -2, 1);
	lua_pushnumber(L, v.x);	lua_rawseti(L, -2, 2);
	return 1;
}

static struct nk_rect* lua_check_nk_rect_ref(lua_State* L, int arg) {
	luaL_argerror(L, arg, "lua_check_nk_rect_ref not implement")
	return NULL;
}

static struct nk_rect lua_check_nk_rect(lua_State* L, int arg) {
	struct nk_rect v;
	luaL_checktype(L, arg, LUA_TTABLE);
	v.x = lua_check_vec_array(L, arg, 1);
	v.y = lua_check_vec_array(L, arg, 2);
	v.w = lua_check_vec_array(L, arg, 3);
	v.h = lua_check_vec_array(L, arg, 4);
	return v;
}

static int lua_push_nk_rect(lua_State* L, struct nk_rect v) {
	lua_createtable(L, 4, 0);
	lua_pushnumber(L, v.x);	lua_rawseti(L, -2, 1);
	lua_pushnumber(L, v.x);	lua_rawseti(L, -2, 2);
	lua_pushnumber(L, v.w);	lua_rawseti(L, -2, 3);
	lua_pushnumber(L, v.h);	lua_rawseti(L, -2, 4);
	return 1;
}

static struct nk_color lua_check_nk_color(lua_State* L, int arg) {
	struct nk_color v;
	luaL_checktype(L, arg, LUA_TTABLE);
	v.r = (nk_byte)lua_check_vec_array(L, arg, 1);
	v.g = (nk_byte)lua_check_vec_array(L, arg, 2);
	v.b = (nk_byte)lua_check_vec_array(L, arg, 3);
	v.a = (nk_byte)lua_check_vec_array(L, arg, 4);
	return v;
}

static struct nk_image lua_check_nk_image(lua_State* L, int arg) {
	struct nk_image img;
	memset(&img, 0, sizeof(nk_image));
	luaL_argerror(L, arg, "lua_check_nk_image not implement");
	return img;
}

static int lua_push_nk_window(lua_State* L, struct nk_window* w) {
	if( w ) {
		lua_pushlightuserdata(L, w);
	} else {
		lua_pushnil(L, w);
	}
	return 1;
}

static int lua_push_nk_panel(lua_State* L, struct nk_panel* p) {
	if( w ) {
		lua_pushlightuserdata(L, p);
	} else {
		lua_pushnil(L, p);
	}
	return 1;
}

static int lua_push_nk_command_buffer(lua_State* L, struct nk_command_buffer* b) {
	if( w ) {
		lua_pushlightuserdata(L, b);
	} else {
		lua_pushnil(L, b);
	}
	return 1;
}

static float* lua_check_nk_float_array(lua_State* L, int arg, int len) {
	luaL_argerror(L, arg, "lua_check_nk_float_array not implement");
	return NULL;
}

static nk_uint* lua_check_nk_uint_ref(lua_State* L, int arg) {
	luaL_argerror(L, arg, "lua_check_nk_uint_ref not implement");
	return NULL;
}

static struct nk_scroll* lua_check_nk_scroll(lua_State* L, int arg) {
	luaL_argerror(L, arg, "lua_check_nk_scroll not implement");
	return NULL;
}

static struct nk_scroll* lua_check_nk_list_view(lua_State* L, int arg) {
	luaL_argerror(L, arg, "lua_check_nk_list_view not implement");
	return NULL;
}


// utils
// 
#define LUA_NK_API(api) static int api ## _lua(lua_State* L)

#define lua_check_nk_enum(type, arg)	(enum type)luaL_checkinteger(L, (arg))
#define lua_check_nk_flags(L, arg)		(nk_flags)luaL_checkinteger((L), (arg))

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR

// NK_API int nk_init_default(struct nk_context*, const struct nk_user_font*);

#endif

// NK_API int nk_init_fixed(struct nk_context*, void *memory, nk_size size, const struct nk_user_font*);

// NK_API int nk_init(struct nk_context*, struct nk_allocator*, const struct nk_user_font*);

// NK_API int nk_init_custom(struct nk_context*, struct nk_buffer *cmds, struct nk_buffer *pool, const struct nk_user_font*);

// NK_API void nk_clear(struct nk_context*);
// 
LUA_NK_API(nk_clear) {
	nk_clear(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_free(struct nk_context*);

#ifdef NK_INCLUDE_COMMAND_USERDATA

// NK_API void nk_set_user_data(struct nk_context*, nk_handle handle);
// 
LUA_NK_API(nk_set_user_data) {
	nk_handle handle;
	if( lua_gettop(L) < 2 ) { lua_pushnil(L); }
	lua_getuservalue(L, 1);
	lua_pushvalue(L, 2);
	handle.id = luaL_ref(L, -2);
	nk_set_user_data(lua_check_nk_context(L, 1), handle);
	return 0;
}

#endif


// NK_API void nk_input_begin(struct nk_context*);
// 
LUA_NK_API(nk_input_begin) {
	nk_input_begin(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_input_motion(struct nk_context*, int x, int y);
// 
LUA_NK_API(nk_input_motion) {
	nk_input_motion(lua_check_nk_context(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3));
	return 0;
}

// NK_API void nk_input_key(struct nk_context*, enum nk_keys, int down);
// 
LUA_NK_API(nk_input_key) {
	nk_input_key(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_keys, 2), lua_toboolean(L, 3));
	return 0;
}

// NK_API void nk_input_button(struct nk_context*, enum nk_buttons, int x, int y, int down);
// 
LUA_NK_API(nk_input_button) {
	nk_input_button(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_buttons, 2)
		, (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4)
		, lua_toboolean(L, 5));
	return 0;
}

// NK_API void nk_input_scroll(struct nk_context*, struct nk_vec2 val);
// 
LUA_NK_API(nk_input_scroll) {
	nk_input_scroll(lua_check_nk_context(L, 1), lua_check_nk_vec2(L, 2));
	return 0;
}

// NK_API void nk_input_char(struct nk_context*, char);
// 
LUA_NK_API(nk_input_char) {
	char ch = 0;
	if( lua_isinteger(L, 2) ) {
		ch = (char)lua_tointeger(L, 2);
	} else {
		size_t n = 0;
		const char* s = luaL_checklstring(L, 2, &n);
		luaL_argcheck(L, n==1, 2, "need char, 1 byte string");
		ch = *s;
	}
	nk_input_char(lua_check_nk_context(L, 1), ch);
	return 0;
}

// NK_API void nk_input_glyph(struct nk_context*, const nk_glyph);
// 
LUA_NK_API(nk_input_glyph) {
	size_t n = 0;
	const char* s = luaL_checklstring(L, 2, &n);
	luaL_argcheck(L, n==NK_UTF_SIZE , 2, "need glyph, 4 bytes string");
	nk_input_glyph(lua_check_nk_context(L, 1), s);
	return 0;
}

// NK_API void nk_input_unicode(struct nk_context*, nk_rune);
// 
LUA_NK_API(nk_input_unicode) {
	nk_input_unicode(lua_check_nk_context(L, 1), (nk_rune)luaL_checkinteger(L, 2));
	return 0;
}

// NK_API void nk_input_end(struct nk_context*);
// 
LUA_NK_API(nk_input_end) {
	nk_input_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API const struct nk_command* nk__begin(struct nk_context*);

// NK_API const struct nk_command* nk__next(struct nk_context*, const struct nk_command*);

// #define nk_foreach(c, ctx) for((c) = nk__begin(lua_check_nk_context(L, 1)); (c) != 0; (c) = nk__next(lua_check_nk_context(L, 1),c))
#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT

// NK_API nk_flags nk_convert(struct nk_context*, struct nk_buffer *cmds, struct nk_buffer *vertices, struct nk_buffer *elements, const struct nk_convert_config*);

// NK_API const struct nk_draw_command* nk__draw_begin(const struct nk_context*, const struct nk_buffer*);

// NK_API const struct nk_draw_command* nk__draw_end(const struct nk_context*, const struct nk_buffer*);

// NK_API const struct nk_draw_command* nk__draw_next(const struct nk_draw_command*, const struct nk_buffer*, const struct nk_context*);

// #define nk_draw_foreach(cmd,ctx, b) for((cmd)=nk__draw_begin(lua_check_nk_context(L, 1), b); (cmd)!=0; (cmd)=nk__draw_next(cmd, b, ctx))
#endif

// NK_API int nk_begin(struct nk_context *ctx, const char *title, struct nk_rect bounds, nk_flags flags);
// 
LUA_NK_API(nk_begin) {
	lua_pushboolean(L, nk_begin(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_rect(L, 3), lua_check_nk_flags(L, 4)));
	return 1;
}

// NK_API int nk_begin_titled(struct nk_context *ctx, const char *name, const char *title, struct nk_rect bounds, nk_flags flags);
// 
LUA_NK_API(nk_begin_titled) {
	lua_pushboolean(L, nk_begin_titled(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3), lua_check_nk_rect(L, 4), lua_check_nk_flags(L, 5)));
	return 1;
}

// NK_API void nk_end(struct nk_context *ctx);
// 
LUA_NK_API(nk_end) {
	nk_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API struct nk_window *nk_window_find(struct nk_context *ctx, const char *name);
// 
LUA_NK_API(nk_window_find) {
	return lua_push_nk_window(L, nk_window_find(lua_check_nk_context(L, 1), luaL_checkstring(L, 2)));
}

// NK_API struct nk_rect nk_window_get_bounds(const struct nk_context *ctx)
// 
LUA_NK_API(nk_window_get_bounds) {
	return lua_push_nk_rect(L, nk_window_get_bounds(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_window_get_position(const struct nk_context *ctx);
// 
LUA_NK_API(nk_window_get_bounds) {
	return lua_push_nk_vec2(L, nk_window_get_position(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_window_get_size(const struct nk_context*);
// 
LUA_NK_API(nk_window_get_size) {
	return lua_push_nk_vec2(L, nk_window_get_size(lua_check_nk_context(L, 1)));
}

// NK_API float nk_window_get_width(const struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_pushnumber(L, nk_window_get_width(lua_check_nk_context(L, 1)));
}

// NK_API float nk_window_get_height(const struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_pushnumber(L, nk_window_get_height(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_panel* nk_window_get_panel(struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_push_nk_panel(L, nk_window_get_panel(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_rect nk_window_get_content_region(struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_push_nk_rect(L, nk_window_get_content_region(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_window_get_content_region_min(struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_push_nk_vec2(L, nk_window_get_content_region_min(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_window_get_content_region_max(struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_push_nk_vec2(L, nk_window_get_content_region_max(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_window_get_content_region_size(struct nk_context*);
// 
LUA_NK_API(nk_window_get_width) {
	return lua_push_nk_vec2(L, nk_window_get_content_region_size(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_command_buffer* nk_window_get_canvas(struct nk_context*);
// 
LUA_NK_API(nk_window_get_canvas) {
	return lua_push_nk_command_buffer(L, nk_window_get_canvas(lua_check_nk_context(L, 1)));
}

// NK_API int nk_window_has_focus(const struct nk_context*);
// 
LUA_NK_API(nk_window_has_focus) {
	return lua_pushboolean(L, nk_window_has_focus(lua_check_nk_context(L, 1)));
}

// NK_API int nk_window_is_collapsed(struct nk_context *ctx, const char *name);
// 
LUA_NK_API(nk_window_is_collapsed) {
	return lua_pushboolean(L, nk_window_is_collapsed(lua_check_nk_context(L, 1), luaL_checkstring(L, 2)));
}

// NK_API int nk_window_is_closed(struct nk_context*, const char*);
// 
LUA_NK_API(nk_window_is_closed) {
	return lua_pushboolean(L, nk_window_is_closed(lua_check_nk_context(L, 1), luaL_checkstring(L, 2)));
}

// NK_API int nk_window_is_hidden(struct nk_context*, const char*);
// 
LUA_NK_API(nk_window_is_hidden) {
	return lua_pushboolean(L, nk_window_is_hidden(lua_check_nk_context(L, 1), luaL_checkstring(L, 2)));
}

// NK_API int nk_window_is_active(struct nk_context*, const char*);
// 
LUA_NK_API(nk_window_is_active) {
	return lua_pushboolean(L, nk_window_is_active(lua_check_nk_context(L, 1), luaL_checkstring(L, 2)));
}

// NK_API int nk_window_is_hovered(struct nk_context*);
// 
LUA_NK_API(nk_window_is_active) {
	return lua_pushboolean(L, nk_window_is_hovered(lua_check_nk_context(L, 1)));
}

// NK_API int nk_window_is_any_hovered(struct nk_context*);
// 
LUA_NK_API(nk_window_is_any_hovered) {
	return lua_pushboolean(L, nk_window_is_any_hovered(lua_check_nk_context(L, 1)));
}

// NK_API int nk_item_is_any_active(struct nk_context*);
// 
LUA_NK_API(nk_item_is_any_active) {
	return lua_pushboolean(L, nk_item_is_any_active(lua_check_nk_context(L, 1)));
}

// NK_API void nk_window_set_bounds(struct nk_context*, const char *name, struct nk_rect bounds);
// 
LUA_NK_API(nk_window_set_bounds) {
	nk_window_set_bounds(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_rect(L, 3));
	return 0;
}

// NK_API void nk_window_set_position(struct nk_context*, const char *name, struct nk_vec2 pos);
// 
LUA_NK_API(nk_window_set_position) {
	nk_window_set_position(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_vec2(L, 3));
	return 0;
}

// NK_API void nk_window_set_size(struct nk_context*, const char *name, struct nk_vec2);
// 
LUA_NK_API(nk_window_set_size) {
	nk_window_set_size(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_vec2(L, 3));
	return 0;
}

// NK_API void nk_window_set_focus(struct nk_context*, const char *name);
// 
LUA_NK_API(nk_window_set_focus) {
	nk_window_set_focus(lua_check_nk_context(L, 1), luaL_checkstring(L, 2));
	return 0;
}

// NK_API void nk_window_close(struct nk_context *ctx, const char *name);
// 
LUA_NK_API(nk_window_close) {
	nk_window_close(lua_check_nk_context(L, 1), luaL_checkstring(L, 2));
	return 0;
}

// NK_API void nk_window_collapse(struct nk_context*, const char *name, enum nk_collapse_states state);
// 
LUA_NK_API(nk_window_collapse) {
	nk_window_collapse(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_enum(nk_collapse_states, 3));
	return 0;
}

// NK_API void nk_window_collapse_if(struct nk_context*, const char *name, enum nk_collapse_states, int cond);

// NK_API void nk_window_show(struct nk_context*, const char *name, enum nk_show_states);
// 
LUA_NK_API(nk_window_show) {
	nk_window_show(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_enum(nk_show_states, 3));
	return 0;
}

// NK_API void nk_window_show_if(struct nk_context*, const char *name, enum nk_show_states, int cond);

// NK_API void nk_layout_set_min_row_height(struct nk_context*, float height);
// 
LUA_NK_API(nk_layout_set_min_row_height) {
	nk_layout_set_min_row_height(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_reset_min_row_height(struct nk_context*);
// 
LUA_NK_API(nk_layout_reset_min_row_height) {
	nk_layout_reset_min_row_height(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API struct nk_rect nk_layout_widget_bounds(struct nk_context*);
// 
LUA_NK_API(nk_layout_widget_bounds) {
	return lua_push_nk_rect(L, nk_layout_widget_bounds(lua_check_nk_context(L, 1)));
}

// NK_API float nk_layout_ratio_from_pixel(struct nk_context*, float pixel_width);
// 
LUA_NK_API(nk_layout_ratio_from_pixel) {
	return lua_pushnumber(L, nk_layout_ratio_from_pixel(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2)));
}

// NK_API void nk_layout_row_dynamic(struct nk_context *ctx, float height, int cols);
// 
LUA_NK_API(nk_layout_row_dynamic) {
	nk_layout_row_dynamic(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2), luaL_checkinteger(L, 3));
	return 0;
}

// NK_API void nk_layout_row_static(struct nk_context *ctx, float height, int item_width, int cols);
// 
LUA_NK_API(nk_layout_row_static) {
	nk_layout_row_static(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2), luaL_checkinteger(L, 3), luaL_checkinteger(L, 4));
	return 0;
}

// NK_API void nk_layout_row_begin(struct nk_context *ctx, enum nk_layout_format fmt, float row_height, int cols);
// 
LUA_NK_API(nk_layout_row_begin) {
	nk_layout_row_begin(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_layout_format, 2), (float)luaL_checknumber(L, 3), luaL_checkinteger(L, 4));
	return 0;
}

// NK_API void nk_layout_row_push(struct nk_context*, float value);
// 
LUA_NK_API(nk_layout_row_push) {
	nk_layout_row_push(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_row_end(struct nk_context*);
// 
LUA_NK_API(nk_layout_row_end) {
	nk_layout_row_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_layout_row(struct nk_context*, enum nk_layout_format, float height, int cols, const float *ratio);
// 
LUA_NK_API(nk_layout_row) {
	int cols = (int)luaL_checkinteger(L, 4);
	nk_layout_row(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_layout_format, 2), (float)luaL_checknumber(L, 3)
		, cols, lua_check_nk_float_array(L, 5, cols));
	return 0;
}

// NK_API void nk_layout_row_template_begin(struct nk_context*, float row_height);
// 
LUA_NK_API(nk_layout_row_template_begin) {
	nk_layout_row_template_begin(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_row_template_push_dynamic(struct nk_context*);
// 
LUA_NK_API(nk_layout_row_template_push_dynamic) {
	nk_layout_row_template_push_dynamic(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_layout_row_template_push_variable(struct nk_context*, float min_width);
// 
LUA_NK_API(nk_layout_row_template_push_variable) {
	nk_layout_row_template_push_variable(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_row_template_push_static(struct nk_context*, float width);
// 
LUA_NK_API(nk_layout_row_template_push_static) {
	nk_layout_row_template_push_static(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_row_template_end(struct nk_context*);
// 
LUA_NK_API(nk_layout_row_template_end) {
	nk_layout_row_template_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_layout_space_begin(struct nk_context*, enum nk_layout_format, float height, int widget_count);
// 
LUA_NK_API(nk_layout_space_begin) {
	nk_layout_space_begin(lua_check_nk_context(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}

// NK_API void nk_layout_space_push(struct nk_context*, struct nk_rect);
// 
LUA_NK_API(nk_layout_space_push) {
	nk_layout_space_push(lua_check_nk_context(L, 1), lua_check_nk_rect(L, 2));
	return 0;
}

// NK_API void nk_layout_space_end(struct nk_context*);
// 
LUA_NK_API(nk_layout_space_end) {
	nk_layout_space_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API struct nk_rect nk_layout_space_bounds(struct nk_context*);
// 
LUA_NK_API(nk_layout_space_bounds) {
	return lua_push_nk_rect(L, nk_layout_space_end(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_layout_space_to_screen(struct nk_context*, struct nk_vec2);
// 
LUA_NK_API(nk_layout_space_to_screen) {
	return lua_push_nk_vec2(L, nk_layout_space_to_screen(lua_check_nk_context(L, 1), lua_check_nk_vec2(L, 2)));
}

// NK_API struct nk_vec2 nk_layout_space_to_local(struct nk_context*, struct nk_vec2);
// 
LUA_NK_API(nk_layout_space_to_local) {
	return lua_push_nk_vec2(L, nk_layout_space_to_local(lua_check_nk_context(L, 1), lua_check_nk_vec2(L, 2)));
}

// NK_API struct nk_rect nk_layout_space_rect_to_screen(struct nk_context*, struct nk_rect);
// 
LUA_NK_API(nk_layout_space_rect_to_screen) {
	return lua_push_nk_rect(L, nk_layout_space_rect_to_screen(lua_check_nk_context(L, 1), lua_check_nk_rect(L, 2)));
}

// NK_API struct nk_rect nk_layout_space_rect_to_local(struct nk_context*, struct nk_rect);
// 
LUA_NK_API(nk_layout_space_rect_to_local) {
	return lua_push_nk_rect(L, nk_layout_space_rect_to_local(lua_check_nk_context(L, 1), lua_check_nk_rect(L, 2)));
}

// NK_API int nk_group_begin(struct nk_context*, const char *title, nk_flags);
// 
LUA_NK_API(nk_group_begin) {
	return lua_pushboolean(L, nk_group_begin(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_flags(L, 3)));
}

// NK_API int nk_group_scrolled_offset_begin(struct nk_context*, nk_uint *x_offset, nk_uint *y_offset, const char*, nk_flags);
// 
LUA_NK_API(nk_group_scrolled_offset_begin) {
	return lua_pushboolean(L, nk_group_scrolled_offset_begin(lua_check_nk_context(L, 1), lua_check_nk_uint_ref(L, 2)
		, lua_check_nk_uint_ref(L, 3), luaL_checkstring(L, 4), lua_check_nk_flags(L, 5)
		));
}

// NK_API int nk_group_scrolled_begin(struct nk_context*, struct nk_scroll*, const char *title, nk_flags);
// 
LUA_NK_API(nk_group_scrolled_begin) {
	return lua_pushboolean(L, nk_group_scrolled_begin(lua_check_nk_context(L, 1), lua_check_nk_scroll(L, 2)
		, luaL_checkstring(L, 3), lua_check_nk_flags(L, 4)
		));
}

// NK_API void nk_group_scrolled_end(struct nk_context*);
// 
LUA_NK_API(nk_group_scrolled_end) {
	nk_group_scrolled_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API void nk_group_end(struct nk_context*);
// 
LUA_NK_API(nk_group_end) {
	nk_group_end(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API int nk_list_view_begin(struct nk_context*, struct nk_list_view *out, const char *id, nk_flags, int row_height, int row_count);
// 
LUA_NK_API(nk_list_view_begin) {
	return lua_pushboolean(L, nk_list_view_begin(lua_check_nk_context(L, 1), lua_check_nk_list_view(L, 2)
		, luaL_checkstring(L, 3), lua_check_nk_flags(L, 4)
		, (int)luaL_checkinteger(L, 5), (int)luaL_checkinteger(L, 6)
		));
}

// NK_API void nk_list_view_end(struct nk_list_view*);
// 
LUA_NK_API(nk_list_view_end) {
	nk_list_view_end(lua_check_nk_context(L, 1));
	return 0;
}

// TODO if need
// #define nk_tree_push(lua_check_nk_context(L, 1), type, title, state) nk_tree_push_hashed(lua_check_nk_context(L, 1), type, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
// #define nk_tree_push_id(lua_check_nk_context(L, 1), type, title, state, id) nk_tree_push_hashed(lua_check_nk_context(L, 1), type, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)

// NK_API int nk_tree_push_hashed(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states initial_state, const char *hash, int len,int seed);
// 
LUA_NK_API(nk_tree_push_hashed) {
	size_t len = 0;
	const char* hash = luaL_checklstring(L, 5, &len);
	return lua_pushboolean(L, nk_tree_push_hashed(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_tree_type, 2), luaL_checkstring(L, 3)
		, lua_check_nk_enum(nk_collapse_states, 4), hash, (int)len, (int)luaL_checkinteger(L, 6)
		));
}

// TODO if need
#define nk_tree_image_push(ctx, type, img, title, state) nk_tree_image_push_hashed(ctx, type, img, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
#define nk_tree_image_push_id(ctx, type, img, title, state, id) nk_tree_image_push_hashed(ctx, type, img, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)

// NK_API int nk_tree_image_push_hashed(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states initial_state, const char *hash, int len,int seed);
// 
LUA_NK_API(nk_tree_image_push_hashed) {
	size_t len = 0;
	const char* hash = luaL_checklstring(L, 6, &len);
	return lua_pushboolean(L, nk_tree_image_push_hashed(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_tree_type, 2)
		, lua_check_nk_image(L, 3), luaL_checkstring(L, 4)
		, lua_check_nk_enum(nk_collapse_states, 5)
		, hash, (int)len
		, (int)luaL_checkinteger(L, 7)
		));
}

// NK_API void nk_tree_pop(struct nk_context*);
// 
LUA_NK_API(nk_tree_pop) {
	nk_tree_pop(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API int nk_tree_state_push(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states *state);
// 
LUA_NK_API(nk_tree_state_push) {
	size_t len = 0;
	const char* hash = luaL_checklstring(L, 6, &len);
	return lua_pushboolean(L, nk_tree_state_push(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_tree_type, 2)
		, luaL_checkstring(L, 3)
		, lua_check_nk_enum(nk_collapse_states, 4)
		));
}

// NK_API int nk_tree_state_image_push(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states *state);
// 
LUA_NK_API(nk_tree_state_image_push) {
	size_t len = 0;
	const char* hash = luaL_checklstring(L, 6, &len);
	return lua_pushboolean(L, nk_tree_state_image_push(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_tree_type, 2)
		, lua_check_nk_image(L, 3), luaL_checkstring(L, 4)
		, lua_check_nk_enum(nk_collapse_states, 5)
		));
}

// NK_API void nk_tree_state_pop(struct nk_context*);
// 
LUA_NK_API(nk_tree_state_pop) {
	nk_tree_state_pop(lua_check_nk_context(L, 1));
	return 0;
}

// NK_API enum nk_widget_layout_states nk_widget(struct nk_rect*, const struct nk_context*);
// 
LUA_NK_API(nk_widget) {
	lua_pushinteger(L, nk_widget(lua_check_nk_context(L, 1), lua_check_nk_rect_ref(L, 1), lua_check_nk_context(L, 2)));
	return 1;
}

// NK_API enum nk_widget_layout_states nk_widget_fitting(struct nk_rect*, struct nk_context*, struct nk_vec2);
// 
LUA_NK_API(nk_widget) {
	lua_pushinteger(L, nk_widget(lua_check_nk_context(L, 1), lua_check_nk_rect_ref(L, 1), lua_check_nk_context(L, 2), lua_check_nk_rect(L, 3)));
	return 1;
}

// NK_API struct nk_rect nk_widget_bounds(struct nk_context*);
// 
LUA_NK_API(nk_widget_bounds) {
	return lua_push_nk_rect(L, nk_widget_bounds(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_widget_position(struct nk_context*);
// 
LUA_NK_API(nk_widget_position) {
	return lua_push_nk_vec2(L, nk_widget_position(lua_check_nk_context(L, 1)));
}

// NK_API struct nk_vec2 nk_widget_size(struct nk_context*);
// 
LUA_NK_API(nk_widget_size) {
	return lua_push_nk_vec2(L, nk_widget_size(lua_check_nk_context(L, 1)));
}

// NK_API float nk_widget_width(struct nk_context*);
// 
LUA_NK_API(nk_widget_width) {
	lua_pushnumber(L, nk_widget_width(lua_check_nk_context(L, 1)));
	return 1;
}

// NK_API float nk_widget_height(struct nk_context*);
// 
LUA_NK_API(nk_widget_height) {
	lua_pushnumber(L, nk_widget_height(lua_check_nk_context(L, 1)));
	return 1;
}

// NK_API int nk_widget_is_hovered(struct nk_context*);
// 
LUA_NK_API(nk_widget_height) {
	lua_pushboolean(L, nk_widget_is_hovered(lua_check_nk_context(L, 1)));
	return 1;
}

// NK_API int nk_widget_is_mouse_clicked(struct nk_context*, enum nk_buttons);
// 
LUA_NK_API(nk_widget_is_mouse_clicked) {
	lua_pushboolean(L, nk_widget_is_mouse_clicked(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_buttons, 2)));
	return 1;
}

// NK_API int nk_widget_has_mouse_click_down(struct nk_context*, enum nk_buttons, int down);
// 
LUA_NK_API(nk_widget_has_mouse_click_down) {
	lua_pushboolean(L, nk_widget_has_mouse_click_down(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_buttons, 2), lua_toboolean(L, 3)));
	return 1;
}

// NK_API void nk_spacing(struct nk_context*, int cols);
// 
LUA_NK_API(nk_spacing) {
	nk_spacing(lua_check_nk_context(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}

// NK_API void nk_text(struct nk_context*, const char*, int, nk_flags);
// 
LUA_NK_API(nk_text) {
	nk_text(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3), lua_check_nk_flags(L, 4));
	return 0;
}

// NK_API void nk_text_colored(struct nk_context*, const char*, int, nk_flags, struct nk_color);
// 
LUA_NK_API(nk_text_colored) {
	nk_text_colored(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3), lua_check_nk_flags(L, 4), lua_check_nk_color(L, 5));
	return 0;
}

// NK_API void nk_text_wrap(struct nk_context*, const char*, int);
// 
LUA_NK_API(nk_text_wrap) {
	nk_text_wrap(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3));
	return 0;
}

// NK_API void nk_text_wrap_colored(struct nk_context*, const char*, int, struct nk_color);
// 
LUA_NK_API(nk_text_wrap_colored) {
	nk_text_wrap_colored(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3), lua_check_nk_color(L, 4));
	return 0;
}

// NK_API void nk_label(struct nk_context*, const char*, nk_flags align);
// 
LUA_NK_API(nk_label) {
	nk_label(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_flags(L, 3));
	return 0;
}

// NK_API void nk_label_colored(struct nk_context*, const char*, nk_flags align, struct nk_color);
// 
LUA_NK_API(nk_label_colored) {
	nk_label_colored(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_flags(L, 3), lua_check_nk_color(L, 4));
	return 0;
}

// NK_API void nk_label_wrap(struct nk_context*, const char*);
// 
LUA_NK_API(nk_label_wrap) {
	nk_label_wrap(lua_check_nk_context(L, 1), luaL_checkstring(L, 2));
	return 0;
}

// NK_API void nk_label_colored_wrap(struct nk_context*, const char*, struct nk_color);
// 
LUA_NK_API(nk_label_colored_wrap) {
	nk_label_colored_wrap(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_color(L, 3));
	return 0;
}

// NK_API void nk_image(struct nk_context*, struct nk_image);
// 
LUA_NK_API(nk_image) {
	nk_image(lua_check_nk_context(L, 1), lua_check_nk_image(L, 2));
	return 0;
}

#ifdef NK_INCLUDE_STANDARD_VARARGS
// NK_API void nk_labelf(struct nk_context*, nk_flags, const char*, ...);
// NK_API void nk_labelf_colored(struct nk_context*, nk_flags align, struct nk_color, const char*,...);
// NK_API void nk_labelf_wrap(struct nk_context*, const char*,...);
// NK_API void nk_labelf_colored_wrap(struct nk_context*, struct nk_color, const char*,...);

// NK_API void nk_value_bool(struct nk_context*, const char *prefix, int);
// 
LUA_NK_API(nk_value_bool) {
	nk_value_bool(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_toboolean(L, 3));
	return 0;
}

// NK_API void nk_value_int(struct nk_context*, const char *prefix, int);
// 
LUA_NK_API(nk_value_int) {
	nk_value_int(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3));
	return 0;
}

// NK_API void nk_value_uint(struct nk_context*, const char *prefix, unsigned int);
// 
LUA_NK_API(nk_value_uint) {
	nk_value_uint(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (unsigned int)luaL_checkinteger(L, 3));
	return 0;
}

// NK_API void nk_value_float(struct nk_context*, const char *prefix, float);
// 
LUA_NK_API(nk_value_float) {
	nk_value_float(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), (float)luaL_checknumber(L, 3));
	return 0;
}

// NK_API void nk_value_color_byte(struct nk_context*, const char *prefix, struct nk_color);
// 
LUA_NK_API(nk_value_color_byte) {
	nk_value_color_byte(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_color(L, 3));
	return 0;
}

// NK_API void nk_value_color_float(struct nk_context*, const char *prefix, struct nk_color);
// 
LUA_NK_API(nk_value_color_float) {
	nk_value_color_float(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_color(L, 3));
	return 0;
}

// NK_API void nk_value_color_hex(struct nk_context*, const char *prefix, struct nk_color);
// 
LUA_NK_API(nk_value_color_hex) {
	nk_value_color_float(lua_check_nk_context(L, 1), luaL_checkstring(L, 2), lua_check_nk_color(L, 3));
	return 0;
}
#endif

// NK_API int nk_button_text(struct nk_context*, const char *title, int len);
// 
LUA_NK_API(nk_button_text) {
	size_t len = 0;
	const char* title = luaL_checklstring(L, 2, &len)
	lua_pushboolean(L, nk_button_text(lua_check_nk_context(L, 1), title, (int)luaL_optinteger(L, 3, (lua_Integer)len)));
	return 1;
}

// NK_API int nk_button_label(struct nk_context*, const char *title);
// 
LUA_NK_API(nk_button_label) {
	lua_pushboolean(L, nk_button_label(lua_check_nk_context(L, 1), luaL_checklstring(L, 2)));
	return 1;
}

// NK_API int nk_button_color(struct nk_context*, struct nk_color);
// 
LUA_NK_API(nk_button_color) {
	lua_pushboolean(L, nk_button_color(lua_check_nk_context(L, 1), lua_check_nk_color(L, 2)));
	return 1;
}

// NK_API int nk_button_symbol(struct nk_context*, enum nk_symbol_type);
// 
LUA_NK_API(nk_button_symbol) {
	lua_pushboolean(L, nk_button_symbol(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_symbol_type, 2)));
	return 1;
}

// NK_API int nk_button_image(struct nk_context*, struct nk_image img);
// 
LUA_NK_API(nk_button_image) {
	lua_pushboolean(L, nk_button_image(lua_check_nk_context(L, 1), lua_check_nk_image(L, 2)));
	return 1;
}

// NK_API int nk_button_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags text_alignment);
// 
LUA_NK_API(nk_button_symbol_label) {
	lua_pushboolean(L, nk_button_symbol_label(lua_check_nk_context(L, 1), lua_check_nk_image(L, 2)));
	return 1;
}

// NK_API int nk_button_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
// 
LUA_NK_API(nk_button_symbol_text) {
	lua_pushboolean(L, nk_button_symbol_text(lua_check_nk_context(L, 1), lua_check_nk_enum(nk_symbol_type, 2), luaL_checkstring(L, 3), lua_check_nk_flags(L, 4)));
	return 1;
}

// NK_API int nk_button_image_label(struct nk_context*, struct nk_image img, const char*, nk_flags text_alignment);
// 
LUA_NK_API(nk_button_image_label) {
	lua_pushboolean(L, nk_button_image_label(lua_check_nk_context(L, 1), lua_check_nk_image(L, 2), luaL_checkstring(L, 3), lua_check_nk_flags(L, 4)));
	return 1;
}

// NK_API int nk_button_image_text(struct nk_context*, struct nk_image img, const char*, int, nk_flags alignment);
// 
LUA_NK_API(nk_button_image_text) {
	size_t n = 0;
	const char* s = luaL_checklstring(L, 3, &len);
	int len = (int)luaL_checkinteger(L, 4);
	if( (len < 0) || ((size_t)len > n) ) { len = (int)n; }
	lua_pushboolean(L, nk_button_image_text(lua_check_nk_context(L, 1), lua_check_nk_image(L, 2), s, len, lua_check_nk_flags(L, 5)));
	return 1;
}

// TODO :
// NK_API int nk_button_text_styled(struct nk_context*, const struct nk_style_button*, const char *title, int len);
// NK_API int nk_button_label_styled(struct nk_context*, const struct nk_style_button*, const char *title);
// NK_API int nk_button_symbol_styled(struct nk_context*, const struct nk_style_button*, enum nk_symbol_type);
// NK_API int nk_button_image_styled(struct nk_context*, const struct nk_style_button*, struct nk_image img);
// NK_API int nk_button_symbol_text_styled(struct nk_context*,const struct nk_style_button*, enum nk_symbol_type, const char*, int, nk_flags alignment);
// NK_API int nk_button_symbol_label_styled(struct nk_context *ctx, const struct nk_style_button *style, enum nk_symbol_type symbol, const char *title, nk_flags align);
// NK_API int nk_button_image_label_styled(struct nk_context*,const struct nk_style_button*, struct nk_image img, const char*, nk_flags text_alignment);
// NK_API int nk_button_image_text_styled(struct nk_context*,const struct nk_style_button*, struct nk_image img, const char*, int, nk_flags alignment);

// NK_API void nk_button_set_behavior(struct nk_context*, enum nk_button_behavior);
// NK_API int nk_button_push_behavior(struct nk_context*, enum nk_button_behavior);
// NK_API int nk_button_pop_behavior(struct nk_context*);

// NK_API int nk_check_label(struct nk_context*, const char*, int active);
// NK_API int nk_check_text(struct nk_context*, const char*, int,int active);
// NK_API unsigned nk_check_flags_label(struct nk_context*, const char*, unsigned int flags, unsigned int value);
// NK_API unsigned nk_check_flags_text(struct nk_context*, const char*, int, unsigned int flags, unsigned int value);
// NK_API int nk_checkbox_label(struct nk_context*, const char*, int *active);
// NK_API int nk_checkbox_text(struct nk_context*, const char*, int, int *active);
// NK_API int nk_checkbox_flags_label(struct nk_context*, const char*, unsigned int *flags, unsigned int value);
// NK_API int nk_checkbox_flags_text(struct nk_context*, const char*, int, unsigned int *flags, unsigned int value);

// NK_API int nk_radio_label(struct nk_context*, const char*, int *active);
// NK_API int nk_radio_text(struct nk_context*, const char*, int, int *active);
// NK_API int nk_option_label(struct nk_context*, const char*, int active);
// NK_API int nk_option_text(struct nk_context*, const char*, int, int active);

// NK_API int nk_selectable_label(struct nk_context*, const char*, nk_flags align, int *value);
// NK_API int nk_selectable_text(struct nk_context*, const char*, int, nk_flags align, int *value);
// NK_API int nk_selectable_image_label(struct nk_context*,struct nk_image,  const char*, nk_flags align, int *value);
// NK_API int nk_selectable_image_text(struct nk_context*,struct nk_image, const char*, int, nk_flags align, int *value);
// NK_API int nk_select_label(struct nk_context*, const char*, nk_flags align, int value);
// NK_API int nk_select_text(struct nk_context*, const char*, int, nk_flags align, int value);
// NK_API int nk_select_image_label(struct nk_context*, struct nk_image,const char*, nk_flags align, int value);
// NK_API int nk_select_image_text(struct nk_context*, struct nk_image,const char*, int, nk_flags align, int value);

// NK_API float nk_slide_float(struct nk_context*, float min, float val, float max, float step);
// NK_API int nk_slide_int(struct nk_context*, int min, int val, int max, int step);
// NK_API int nk_slider_float(struct nk_context*, float min, float *val, float max, float step);
// NK_API int nk_slider_int(struct nk_context*, int min, int *val, int max, int step);

// NK_API int nk_progress(struct nk_context*, nk_size *cur, nk_size max, int modifyable);
// NK_API nk_size nk_prog(struct nk_context*, nk_size cur, nk_size max, int modifyable);


// NK_API struct nk_color nk_color_picker(struct nk_context*, struct nk_color, enum nk_color_format);
// NK_API int nk_color_pick(struct nk_context*, struct nk_color*, enum nk_color_format);

// NK_API void nk_property_int(struct nk_context*, const char *name, int min, int *val, int max, int step, float inc_per_pixel);
// NK_API void nk_property_float(struct nk_context*, const char *name, float min, float *val, float max, float step, float inc_per_pixel);
// NK_API void nk_property_double(struct nk_context*, const char *name, double min, double *val, double max, double step, float inc_per_pixel);
// NK_API int nk_propertyi(struct nk_context*, const char *name, int min, int val, int max, int step, float inc_per_pixel);
// NK_API float nk_propertyf(struct nk_context*, const char *name, float min, float val, float max, float step, float inc_per_pixel);
// NK_API double nk_propertyd(struct nk_context*, const char *name, double min, double val, double max, double step, float inc_per_pixel);

// NK_API nk_flags nk_edit_string(struct nk_context*, nk_flags, char *buffer, int *len, int max, nk_plugin_filter);
// NK_API nk_flags nk_edit_string_zero_terminated(struct nk_context*, nk_flags, char *buffer, int max, nk_plugin_filter);
// NK_API nk_flags nk_edit_buffer(struct nk_context*, nk_flags, struct nk_text_edit*, nk_plugin_filter);
// NK_API void nk_edit_focus(struct nk_context*, nk_flags flags);
// NK_API void nk_edit_unfocus(struct nk_context*);

// NK_API int nk_chart_begin(struct nk_context*, enum nk_chart_type, int num, float min, float max);
// NK_API int nk_chart_begin_colored(struct nk_context*, enum nk_chart_type, struct nk_color, struct nk_color active, int num, float min, float max);
// NK_API void nk_chart_add_slot(struct nk_context *ctx, const enum nk_chart_type, int count, float min_value, float max_value);
// NK_API void nk_chart_add_slot_colored(struct nk_context *ctx, const enum nk_chart_type, struct nk_color, struct nk_color active, int count, float min_value, float max_value);
// NK_API nk_flags nk_chart_push(struct nk_context*, float);
// NK_API nk_flags nk_chart_push_slot(struct nk_context*, float, int);
// NK_API void nk_chart_end(struct nk_context*);
// NK_API void nk_plot(struct nk_context*, enum nk_chart_type, const float *values, int count, int offset);
// NK_API void nk_plot_function(struct nk_context*, enum nk_chart_type, void *userdata, float(*value_getter)(void* user, int index), int count, int offset);

// NK_API int nk_popup_begin(struct nk_context*, enum nk_popup_type, const char*, nk_flags, struct nk_rect bounds);
// NK_API void nk_popup_close(struct nk_context*);
// NK_API void nk_popup_end(struct nk_context*);

// NK_API int nk_combo(struct nk_context*, const char **items, int count, int selected, int item_height, struct nk_vec2 size);
// NK_API int nk_combo_separator(struct nk_context*, const char *items_separated_by_separator, int separator, int selected, int count, int item_height, struct nk_vec2 size);
// NK_API int nk_combo_string(struct nk_context*, const char *items_separated_by_zeros, int selected, int count, int item_height, struct nk_vec2 size);
// NK_API int nk_combo_callback(struct nk_context*, void(*item_getter)(void*, int, const char**), void *userdata, int selected, int count, int item_height, struct nk_vec2 size);
// NK_API void nk_combobox(struct nk_context*, const char **items, int count, int *selected, int item_height, struct nk_vec2 size);
// NK_API void nk_combobox_string(struct nk_context*, const char *items_separated_by_zeros, int *selected, int count, int item_height, struct nk_vec2 size);
// NK_API void nk_combobox_separator(struct nk_context*, const char *items_separated_by_separator, int separator,int *selected, int count, int item_height, struct nk_vec2 size);
// NK_API void nk_combobox_callback(struct nk_context*, void(*item_getter)(void*, int, const char**), void*, int *selected, int count, int item_height, struct nk_vec2 size);

// NK_API int nk_combo_begin_text(struct nk_context*, const char *selected, int, struct nk_vec2 size);
// NK_API int nk_combo_begin_label(struct nk_context*, const char *selected, struct nk_vec2 size);
// NK_API int nk_combo_begin_color(struct nk_context*, struct nk_color color, struct nk_vec2 size);
// NK_API int nk_combo_begin_symbol(struct nk_context*,  enum nk_symbol_type,  struct nk_vec2 size);
// NK_API int nk_combo_begin_symbol_label(struct nk_context*, const char *selected, enum nk_symbol_type, struct nk_vec2 size);
// NK_API int nk_combo_begin_symbol_text(struct nk_context*, const char *selected, int, enum nk_symbol_type, struct nk_vec2 size);
// NK_API int nk_combo_begin_image(struct nk_context*, struct nk_image img,  struct nk_vec2 size);
// NK_API int nk_combo_begin_image_label(struct nk_context*, const char *selected, struct nk_image, struct nk_vec2 size);
// NK_API int nk_combo_begin_image_text(struct nk_context*,  const char *selected, int, struct nk_image, struct nk_vec2 size);
// NK_API int nk_combo_item_label(struct nk_context*, const char*, nk_flags alignment);
// NK_API int nk_combo_item_text(struct nk_context*, const char*,int, nk_flags alignment);
// NK_API int nk_combo_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
// NK_API int nk_combo_item_image_text(struct nk_context*, struct nk_image, const char*, int,nk_flags alignment);
// NK_API int nk_combo_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
// NK_API int nk_combo_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
// NK_API void nk_combo_close(struct nk_context*);
// NK_API void nk_combo_end(struct nk_context*);

// NK_API int nk_contextual_begin(struct nk_context*, nk_flags, struct nk_vec2, struct nk_rect trigger_bounds);
// NK_API int nk_contextual_item_text(struct nk_context*, const char*, int,nk_flags align);
// NK_API int nk_contextual_item_label(struct nk_context*, const char*, nk_flags align);
// NK_API int nk_contextual_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
// NK_API int nk_contextual_item_image_text(struct nk_context*, struct nk_image, const char*, int len, nk_flags alignment);
// NK_API int nk_contextual_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
// NK_API int nk_contextual_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
// NK_API void nk_contextual_close(struct nk_context*);
// NK_API void nk_contextual_end(struct nk_context*);

// NK_API void nk_tooltip(struct nk_context*, const char*);
// NK_API int nk_tooltip_begin(struct nk_context*, float width);
// NK_API void nk_tooltip_end(struct nk_context*);

// NK_API void nk_menubar_begin(struct nk_context*);
// NK_API void nk_menubar_end(struct nk_context*);
// NK_API int nk_menu_begin_text(struct nk_context*, const char* title, int title_len, nk_flags align, struct nk_vec2 size);
// NK_API int nk_menu_begin_label(struct nk_context*, const char*, nk_flags align, struct nk_vec2 size);
// NK_API int nk_menu_begin_image(struct nk_context*, const char*, struct nk_image, struct nk_vec2 size);
// NK_API int nk_menu_begin_image_text(struct nk_context*, const char*, int,nk_flags align,struct nk_image, struct nk_vec2 size);
// NK_API int nk_menu_begin_image_label(struct nk_context*, const char*, nk_flags align,struct nk_image, struct nk_vec2 size);
// NK_API int nk_menu_begin_symbol(struct nk_context*, const char*, enum nk_symbol_type, struct nk_vec2 size);
// NK_API int nk_menu_begin_symbol_text(struct nk_context*, const char*, int,nk_flags align,enum nk_symbol_type, struct nk_vec2 size);
// NK_API int nk_menu_begin_symbol_label(struct nk_context*, const char*, nk_flags align,enum nk_symbol_type, struct nk_vec2 size);
// NK_API int nk_menu_item_text(struct nk_context*, const char*, int,nk_flags align);
// NK_API int nk_menu_item_label(struct nk_context*, const char*, nk_flags alignment);
// NK_API int nk_menu_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
// NK_API int nk_menu_item_image_text(struct nk_context*, struct nk_image, const char*, int len, nk_flags alignment);
// NK_API int nk_menu_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
// NK_API int nk_menu_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
// NK_API void nk_menu_close(struct nk_context*);
// NK_API void nk_menu_end(struct nk_context*);

// NK_API void nk_style_default(struct nk_context*);
// NK_API void nk_style_from_table(struct nk_context*, const struct nk_color*);
// NK_API void nk_style_load_cursor(struct nk_context*, enum nk_style_cursor, const struct nk_cursor*);
// NK_API void nk_style_load_all_cursors(struct nk_context*, struct nk_cursor*);
// NK_API const char* nk_style_get_color_by_name(enum nk_style_colors);
// NK_API void nk_style_set_font(struct nk_context*, const struct nk_user_font*);
// NK_API int nk_style_set_cursor(struct nk_context*, enum nk_style_cursor);
// NK_API void nk_style_show_cursor(struct nk_context*);
// NK_API void nk_style_hide_cursor(struct nk_context*);

// NK_API int nk_style_push_font(struct nk_context*, const struct nk_user_font*);
// NK_API int nk_style_push_float(struct nk_context*, float*, float);
// NK_API int nk_style_push_vec2(struct nk_context*, struct nk_vec2*, struct nk_vec2);
// NK_API int nk_style_push_style_item(struct nk_context*, struct nk_style_item*, struct nk_style_item);
// NK_API int nk_style_push_flags(struct nk_context*, nk_flags*, nk_flags);
// NK_API int nk_style_push_color(struct nk_context*, struct nk_color*, struct nk_color);

// NK_API int nk_style_pop_font(struct nk_context*);
// NK_API int nk_style_pop_float(struct nk_context*);
// NK_API int nk_style_pop_vec2(struct nk_context*);
// NK_API int nk_style_pop_style_item(struct nk_context*);
// NK_API int nk_style_pop_flags(struct nk_context*);
// NK_API int nk_style_pop_color(struct nk_context*);

// NK_API struct nk_color nk_rgb(int r, int g, int b);
// NK_API struct nk_color nk_rgb_iv(const int *rgb);
// NK_API struct nk_color nk_rgb_bv(const nk_byte* rgb);
// NK_API struct nk_color nk_rgb_f(float r, float g, float b);
// NK_API struct nk_color nk_rgb_fv(const float *rgb);
// NK_API struct nk_color nk_rgb_hex(const char *rgb);

// NK_API struct nk_color nk_rgba(int r, int g, int b, int a);
// NK_API struct nk_color nk_rgba_u32(nk_uint);
// NK_API struct nk_color nk_rgba_iv(const int *rgba);
// NK_API struct nk_color nk_rgba_bv(const nk_byte *rgba);
// NK_API struct nk_color nk_rgba_f(float r, float g, float b, float a);
// NK_API struct nk_color nk_rgba_fv(const float *rgba);
// NK_API struct nk_color nk_rgba_hex(const char *rgb);

// NK_API struct nk_color nk_hsv(int h, int s, int v);
// NK_API struct nk_color nk_hsv_iv(const int *hsv);
// NK_API struct nk_color nk_hsv_bv(const nk_byte *hsv);
// NK_API struct nk_color nk_hsv_f(float h, float s, float v);
// NK_API struct nk_color nk_hsv_fv(const float *hsv);

// NK_API struct nk_color nk_hsva(int h, int s, int v, int a);
// NK_API struct nk_color nk_hsva_iv(const int *hsva);
// NK_API struct nk_color nk_hsva_bv(const nk_byte *hsva);
// NK_API struct nk_color nk_hsva_f(float h, float s, float v, float a);
// NK_API struct nk_color nk_hsva_fv(const float *hsva);


// NK_API void nk_color_f(float *r, float *g, float *b, float *a, struct nk_color);
// NK_API void nk_color_fv(float *rgba_out, struct nk_color);
// NK_API void nk_color_d(double *r, double *g, double *b, double *a, struct nk_color);
// NK_API void nk_color_dv(double *rgba_out, struct nk_color);

// NK_API nk_uint nk_color_u32(struct nk_color);
// NK_API void nk_color_hex_rgba(char *output, struct nk_color);
// NK_API void nk_color_hex_rgb(char *output, struct nk_color);

// NK_API void nk_color_hsv_i(int *out_h, int *out_s, int *out_v, struct nk_color);
// NK_API void nk_color_hsv_b(nk_byte *out_h, nk_byte *out_s, nk_byte *out_v, struct nk_color);
// NK_API void nk_color_hsv_iv(int *hsv_out, struct nk_color);
// NK_API void nk_color_hsv_bv(nk_byte *hsv_out, struct nk_color);
// NK_API void nk_color_hsv_f(float *out_h, float *out_s, float *out_v, struct nk_color);
// NK_API void nk_color_hsv_fv(float *hsv_out, struct nk_color);

// NK_API void nk_color_hsva_i(int *h, int *s, int *v, int *a, struct nk_color);
// NK_API void nk_color_hsva_b(nk_byte *h, nk_byte *s, nk_byte *v, nk_byte *a, struct nk_color);
// NK_API void nk_color_hsva_iv(int *hsva_out, struct nk_color);
// NK_API void nk_color_hsva_bv(nk_byte *hsva_out, struct nk_color);
// NK_API void nk_color_hsva_f(float *out_h, float *out_s, float *out_v, float *out_a, struct nk_color);
// NK_API void nk_color_hsva_fv(float *hsva_out, struct nk_color);

// NK_API nk_handle nk_handle_ptr(void*);
// NK_API nk_handle nk_handle_id(int);
// NK_API struct nk_image nk_image_handle(nk_handle);
// NK_API struct nk_image nk_image_ptr(void*);
// NK_API struct nk_image nk_image_id(int);
// NK_API int nk_image_is_subimage(const struct nk_image* img);
// NK_API struct nk_image nk_subimage_ptr(void*, unsigned short w, unsigned short h, struct nk_rect sub_region);
// NK_API struct nk_image nk_subimage_id(int, unsigned short w, unsigned short h, struct nk_rect sub_region);
// NK_API struct nk_image nk_subimage_handle(nk_handle, unsigned short w, unsigned short h, struct nk_rect sub_region);

// NK_API nk_hash nk_murmur_hash(const void *key, int len, nk_hash seed);
// NK_API void nk_triangle_from_direction(struct nk_vec2 *result, struct nk_rect r, float pad_x, float pad_y, enum nk_heading);

// NK_API struct nk_vec2 nk_vec2(float x, float y);
// NK_API struct nk_vec2 nk_vec2i(int x, int y);
// NK_API struct nk_vec2 nk_vec2v(const float *xy);
// NK_API struct nk_vec2 nk_vec2iv(const int *xy);

// NK_API struct nk_rect nk_get_null_rect(void);
// NK_API struct nk_rect nk_rect(float x, float y, float w, float h);
// NK_API struct nk_rect nk_recti(int x, int y, int w, int h);
// NK_API struct nk_rect nk_recta(struct nk_vec2 pos, struct nk_vec2 size);
// NK_API struct nk_rect nk_rectv(const float *xywh);
// NK_API struct nk_rect nk_rectiv(const int *xywh);
// NK_API struct nk_vec2 nk_rect_pos(struct nk_rect);
// NK_API struct nk_vec2 nk_rect_size(struct nk_rect);

// NK_API int nk_strlen(const char *str);
// NK_API int nk_stricmp(const char *s1, const char *s2);
// NK_API int nk_stricmpn(const char *s1, const char *s2, int n);
// NK_API int nk_strtoi(const char *str, const char **endptr);
// NK_API float nk_strtof(const char *str, const char **endptr);
// NK_API double nk_strtod(const char *str, const char **endptr);
// NK_API int nk_strfilter(const char *text, const char *regexp);
// NK_API int nk_strmatch_fuzzy_string(char const *str, char const *pattern, int *out_score);
// NK_API int nk_strmatch_fuzzy_text(const char *txt, int txt_len, const char *pattern, int *out_score);

// NK_API int nk_utf_decode(const char*, nk_rune*, int);
// NK_API int nk_utf_encode(nk_rune, char*, int);
// NK_API int nk_utf_len(const char*, int byte_len);
// NK_API const char* nk_utf_at(const char *buffer, int length, int index, nk_rune *unicode, int *len);

#ifdef NK_INCLUDE_FONT_BAKING

// NK_API const nk_rune *nk_font_default_glyph_ranges(void);
// NK_API const nk_rune *nk_font_chinese_glyph_ranges(void);
// NK_API const nk_rune *nk_font_cyrillic_glyph_ranges(void);
// NK_API const nk_rune *nk_font_korean_glyph_ranges(void);

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API void nk_font_atlas_init_default(struct nk_font_atlas*);
#endif
// NK_API void nk_font_atlas_init(struct nk_font_atlas*, struct nk_allocator*);
// NK_API void nk_font_atlas_init_custom(struct nk_font_atlas*, struct nk_allocator *persistent, struct nk_allocator *transient);
// NK_API void nk_font_atlas_begin(struct nk_font_atlas*);
// NK_API struct nk_font_config nk_font_config(float pixel_height);
// NK_API struct nk_font *nk_font_atlas_add(struct nk_font_atlas*, const struct nk_font_config*);
#ifdef NK_INCLUDE_DEFAULT_FONT
// NK_API struct nk_font* nk_font_atlas_add_default(struct nk_font_atlas*, float height, const struct nk_font_config*);
#endif
// NK_API struct nk_font* nk_font_atlas_add_from_memory(struct nk_font_atlas *atlas, void *memory, nk_size size, float height, const struct nk_font_config *config);
#ifdef NK_INCLUDE_STANDARD_IO
// NK_API struct nk_font* nk_font_atlas_add_from_file(struct nk_font_atlas *atlas, const char *file_path, float height, const struct nk_font_config*);
#endif
// NK_API struct nk_font *nk_font_atlas_add_compressed(struct nk_font_atlas*, void *memory, nk_size size, float height, const struct nk_font_config*);
// NK_API struct nk_font* nk_font_atlas_add_compressed_base85(struct nk_font_atlas*, const char *data, float height, const struct nk_font_config *config);
// NK_API const void* nk_font_atlas_bake(struct nk_font_atlas*, int *width, int *height, enum nk_font_atlas_format);
// NK_API void nk_font_atlas_end(struct nk_font_atlas*, nk_handle tex, struct nk_draw_null_texture*);
// NK_API const struct nk_font_glyph* nk_font_find_glyph(struct nk_font*, nk_rune unicode);
// NK_API void nk_font_atlas_cleanup(struct nk_font_atlas *atlas);
// NK_API void nk_font_atlas_clear(struct nk_font_atlas*);

#endif

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API void nk_buffer_init_default(struct nk_buffer*);
#endif
// NK_API void nk_buffer_init(struct nk_buffer*, const struct nk_allocator*, nk_size size);
// NK_API void nk_buffer_init_fixed(struct nk_buffer*, void *memory, nk_size size);
// NK_API void nk_buffer_info(struct nk_memory_status*, struct nk_buffer*);
// NK_API void nk_buffer_push(struct nk_buffer*, enum nk_buffer_allocation_type type, const void *memory, nk_size size, nk_size align);
// NK_API void nk_buffer_mark(struct nk_buffer*, enum nk_buffer_allocation_type type);
// NK_API void nk_buffer_reset(struct nk_buffer*, enum nk_buffer_allocation_type type);
// NK_API void nk_buffer_clear(struct nk_buffer*);
// NK_API void nk_buffer_free(struct nk_buffer*);
// NK_API void *nk_buffer_memory(struct nk_buffer*);
// NK_API const void *nk_buffer_memory_const(const struct nk_buffer*);
// NK_API nk_size nk_buffer_total(struct nk_buffer*);

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API void nk_str_init_default(struct nk_str*);
#endif
// NK_API void nk_str_init(struct nk_str*, const struct nk_allocator*, nk_size size);
// NK_API void nk_str_init_fixed(struct nk_str*, void *memory, nk_size size);
// NK_API void nk_str_clear(struct nk_str*);
// NK_API void nk_str_free(struct nk_str*);

// NK_API int nk_str_append_text_char(struct nk_str*, const char*, int);
// NK_API int nk_str_append_str_char(struct nk_str*, const char*);
// NK_API int nk_str_append_text_utf8(struct nk_str*, const char*, int);
// NK_API int nk_str_append_str_utf8(struct nk_str*, const char*);
// NK_API int nk_str_append_text_runes(struct nk_str*, const nk_rune*, int);
// NK_API int nk_str_append_str_runes(struct nk_str*, const nk_rune*);

// NK_API int nk_str_insert_at_char(struct nk_str*, int pos, const char*, int);
// NK_API int nk_str_insert_at_rune(struct nk_str*, int pos, const char*, int);

// NK_API int nk_str_insert_text_char(struct nk_str*, int pos, const char*, int);
// NK_API int nk_str_insert_str_char(struct nk_str*, int pos, const char*);
// NK_API int nk_str_insert_text_utf8(struct nk_str*, int pos, const char*, int);
// NK_API int nk_str_insert_str_utf8(struct nk_str*, int pos, const char*);
// NK_API int nk_str_insert_text_runes(struct nk_str*, int pos, const nk_rune*, int);
// NK_API int nk_str_insert_str_runes(struct nk_str*, int pos, const nk_rune*);

// NK_API void nk_str_remove_chars(struct nk_str*, int len);
// NK_API void nk_str_remove_runes(struct nk_str *str, int len);
// NK_API void nk_str_delete_chars(struct nk_str*, int pos, int len);
// NK_API void nk_str_delete_runes(struct nk_str*, int pos, int len);

// NK_API char *nk_str_at_char(struct nk_str*, int pos);
// NK_API char *nk_str_at_rune(struct nk_str*, int pos, nk_rune *unicode, int *len);
// NK_API nk_rune nk_str_rune_at(const struct nk_str*, int pos);
// NK_API const char *nk_str_at_char_const(const struct nk_str*, int pos);
// NK_API const char *nk_str_at_const(const struct nk_str*, int pos, nk_rune *unicode, int *len);

// NK_API char *nk_str_get(struct nk_str*);
// NK_API const char *nk_str_get_const(const struct nk_str*);
// NK_API int nk_str_len(struct nk_str*);
// NK_API int nk_str_len_char(struct nk_str*);

// NK_API int nk_filter_default(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_ascii(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_float(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_decimal(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_hex(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_oct(const struct nk_text_edit*, nk_rune unicode);
// NK_API int nk_filter_binary(const struct nk_text_edit*, nk_rune unicode);


#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API void nk_textedit_init_default(struct nk_text_edit*);
#endif
// NK_API void nk_textedit_init(struct nk_text_edit*, struct nk_allocator*, nk_size size);
// NK_API void nk_textedit_init_fixed(struct nk_text_edit*, void *memory, nk_size size);
// NK_API void nk_textedit_free(struct nk_text_edit*);
// NK_API void nk_textedit_text(struct nk_text_edit*, const char*, int total_len);
// NK_API void nk_textedit_delete(struct nk_text_edit*, int where, int len);
// NK_API void nk_textedit_delete_selection(struct nk_text_edit*);
// NK_API void nk_textedit_select_all(struct nk_text_edit*);
// NK_API int nk_textedit_cut(struct nk_text_edit*);
// NK_API int nk_textedit_paste(struct nk_text_edit*, char const*, int len);
// NK_API void nk_textedit_undo(struct nk_text_edit*);
// NK_API void nk_textedit_redo(struct nk_text_edit*);

// NK_API void nk_stroke_line(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float line_thickness, struct nk_color);
// NK_API void nk_stroke_curve(struct nk_command_buffer*, float, float, float, float, float, float, float, float, float line_thickness, struct nk_color);
// NK_API void nk_stroke_rect(struct nk_command_buffer*, struct nk_rect, float rounding, float line_thickness, struct nk_color);
// NK_API void nk_stroke_circle(struct nk_command_buffer*, struct nk_rect, float line_thickness, struct nk_color);
// NK_API void nk_stroke_arc(struct nk_command_buffer*, float cx, float cy, float radius, float a_min, float a_max, float line_thickness, struct nk_color);
// NK_API void nk_stroke_triangle(struct nk_command_buffer*, float, float, float, float, float, float, float line_thichness, struct nk_color);
// NK_API void nk_stroke_polyline(struct nk_command_buffer*, float *points, int point_count, float line_thickness, struct nk_color col);
// NK_API void nk_stroke_polygon(struct nk_command_buffer*, float*, int point_count, float line_thickness, struct nk_color);


// NK_API void nk_fill_rect(struct nk_command_buffer*, struct nk_rect, float rounding, struct nk_color);
// NK_API void nk_fill_rect_multi_color(struct nk_command_buffer*, struct nk_rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom);
// NK_API void nk_fill_circle(struct nk_command_buffer*, struct nk_rect, struct nk_color);
// NK_API void nk_fill_arc(struct nk_command_buffer*, float cx, float cy, float radius, float a_min, float a_max, struct nk_color);
// NK_API void nk_fill_triangle(struct nk_command_buffer*, float x0, float y0, float x1, float y1, float x2, float y2, struct nk_color);
// NK_API void nk_fill_polygon(struct nk_command_buffer*, float*, int point_count, struct nk_color);


// NK_API void nk_draw_image(struct nk_command_buffer*, struct nk_rect, const struct nk_image*, struct nk_color);
// NK_API void nk_draw_text(struct nk_command_buffer*, struct nk_rect, const char *text, int len, const struct nk_user_font*, struct nk_color, struct nk_color);
// NK_API void nk_push_scissor(struct nk_command_buffer*, struct nk_rect);
// NK_API void nk_push_custom(struct nk_command_buffer*, struct nk_rect, nk_command_custom_callback, nk_handle usr);

// NK_API int nk_input_has_mouse_click(const struct nk_input*, enum nk_buttons);
// NK_API int nk_input_has_mouse_click_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect);
// NK_API int nk_input_has_mouse_click_down_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect, int down);
// NK_API int nk_input_is_mouse_click_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect);
// NK_API int nk_input_is_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b, int down);
// NK_API int nk_input_any_mouse_click_in_rect(const struct nk_input*, struct nk_rect);
// NK_API int nk_input_is_mouse_prev_hovering_rect(const struct nk_input*, struct nk_rect);
// NK_API int nk_input_is_mouse_hovering_rect(const struct nk_input*, struct nk_rect);
// NK_API int nk_input_mouse_clicked(const struct nk_input*, enum nk_buttons, struct nk_rect);
// NK_API int nk_input_is_mouse_down(const struct nk_input*, enum nk_buttons);
// NK_API int nk_input_is_mouse_pressed(const struct nk_input*, enum nk_buttons);
// NK_API int nk_input_is_mouse_released(const struct nk_input*, enum nk_buttons);
// NK_API int nk_input_is_key_pressed(const struct nk_input*, enum nk_keys);
// NK_API int nk_input_is_key_released(const struct nk_input*, enum nk_keys);
// NK_API int nk_input_is_key_down(const struct nk_input*, enum nk_keys);


#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT

// NK_API void nk_draw_list_init(struct nk_draw_list*);
// NK_API void nk_draw_list_setup(struct nk_draw_list*, const struct nk_convert_config*, struct nk_buffer *cmds, struct nk_buffer *vertices, struct nk_buffer *elements, enum nk_anti_aliasing line_aa,enum nk_anti_aliasing shape_aa);
// NK_API void nk_draw_list_clear(struct nk_draw_list*);


#define nk_draw_list_foreach(cmd, can, b) for((cmd)=nk__draw_list_begin(can, b); (cmd)!=0; (cmd)=nk__draw_list_next(cmd, b, can))
// NK_API const struct nk_draw_command* nk__draw_list_begin(const struct nk_draw_list*, const struct nk_buffer*);
// NK_API const struct nk_draw_command* nk__draw_list_next(const struct nk_draw_command*, const struct nk_buffer*, const struct nk_draw_list*);
// NK_API const struct nk_draw_command* nk__draw_list_end(const struct nk_draw_list*, const struct nk_buffer*);
// NK_API void nk_draw_list_clear(struct nk_draw_list *list);


// NK_API void nk_draw_list_path_clear(struct nk_draw_list*);
// NK_API void nk_draw_list_path_line_to(struct nk_draw_list*, struct nk_vec2 pos);
// NK_API void nk_draw_list_path_arc_to_fast(struct nk_draw_list*, struct nk_vec2 center, float radius, int a_min, int a_max);
// NK_API void nk_draw_list_path_arc_to(struct nk_draw_list*, struct nk_vec2 center, float radius, float a_min, float a_max, unsigned int segments);
// NK_API void nk_draw_list_path_rect_to(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, float rounding);
// NK_API void nk_draw_list_path_curve_to(struct nk_draw_list*, struct nk_vec2 p2, struct nk_vec2 p3, struct nk_vec2 p4, unsigned int num_segments);
// NK_API void nk_draw_list_path_fill(struct nk_draw_list*, struct nk_color);
// NK_API void nk_draw_list_path_stroke(struct nk_draw_list*, struct nk_color, enum nk_draw_list_stroke closed, float thickness);


// NK_API void nk_draw_list_stroke_line(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_color, float thickness);
// NK_API void nk_draw_list_stroke_rect(struct nk_draw_list*, struct nk_rect rect, struct nk_color, float rounding, float thickness);
// NK_API void nk_draw_list_stroke_triangle(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, struct nk_color, float thickness);
// NK_API void nk_draw_list_stroke_circle(struct nk_draw_list*, struct nk_vec2 center, float radius, struct nk_color, unsigned int segs, float thickness);
// NK_API void nk_draw_list_stroke_curve(struct nk_draw_list*, struct nk_vec2 p0, struct nk_vec2 cp0, struct nk_vec2 cp1, struct nk_vec2 p1, struct nk_color, unsigned int segments, float thickness);
// NK_API void nk_draw_list_stroke_poly_line(struct nk_draw_list*, const struct nk_vec2 *pnts, const unsigned int cnt, struct nk_color, enum nk_draw_list_stroke, float thickness, enum nk_anti_aliasing);


// NK_API void nk_draw_list_fill_rect(struct nk_draw_list*, struct nk_rect rect, struct nk_color, float rounding);
// NK_API void nk_draw_list_fill_rect_multi_color(struct nk_draw_list*, struct nk_rect rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom);
// NK_API void nk_draw_list_fill_triangle(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, struct nk_color);
// NK_API void nk_draw_list_fill_circle(struct nk_draw_list*, struct nk_vec2 center, float radius, struct nk_color col, unsigned int segs);
// NK_API void nk_draw_list_fill_poly_convex(struct nk_draw_list*, const struct nk_vec2 *points, const unsigned int count, struct nk_color, enum nk_anti_aliasing);


// NK_API void nk_draw_list_add_image(struct nk_draw_list*, struct nk_image texture, struct nk_rect rect, struct nk_color);
// NK_API void nk_draw_list_add_text(struct nk_draw_list*, const struct nk_user_font*, struct nk_rect, const char *text, int len, float font_height, struct nk_color);
#ifdef NK_INCLUDE_COMMAND_USERDATA
// NK_API void nk_draw_list_push_userdata(struct nk_draw_list*, nk_handle userdata);
#endif

#endif

// NK_API struct nk_style_item nk_style_item_image(struct nk_image img);
// NK_API struct nk_style_item nk_style_item_color(struct nk_color);
// NK_API struct nk_style_item nk_style_item_hide(void);
