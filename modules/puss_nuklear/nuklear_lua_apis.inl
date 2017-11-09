// nuklear_lua_apis.inl

#if defined(USE_LUA_NK_API_REGISTER)
	#define LUA_NK_API_DEFINE( api, ... ) , { #api, api ## _lua }
	#define LUA_NK_API_NORET( api, ... )  LUA_NK_API_DEFINE( api, __VA_ARGS__ )
	#define LUA_NK_API( push, api, ... )  LUA_NK_API_DEFINE( api, __VA_ARGS__ )

#elif defined(USE_LUA_NK_API_IMPLEMENT)
	#define LUA_NK_API_DEFINE( api, ... ) static int api ## _lua(lua_State* L) __VA_ARGS__
	#define LUA_NK_API_NORET( api, ... )  LUA_NK_API_DEFINE( api, { api(__VA_ARGS__); return 0; } )
	#define LUA_NK_API( push, api, ... )  LUA_NK_API_DEFINE( api, { push(L, api(__VA_ARGS__) ); return 1; } )

	// boxed implement
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

	// base types
	#define lua_check_nk_enum(L, arg, T)		(enum T)luaL_checkinteger((L), (arg))
	#define lua_check_nk_flags(L, arg)			(nk_flags)luaL_checkinteger((L), (arg))
	#define lua_check_nk_int(L, arg)			(int)luaL_checkinteger((L), (arg))
	#define lua_check_nk_uint(L, arg)			(nk_uint)luaL_checkinteger((L), (arg))
	#define lua_check_nk_size(L, arg)			(nk_size)luaL_checkinteger((L), (arg))
	#define lua_check_nk_float(L, arg)			(float)luaL_checknumber((L), (arg))
	#define lua_check_nk_double(L, arg)			luaL_checknumber((L), (arg))

	// base boxed
	#define lua_new_nk_boxed(L, T)				(T*)_lua_new_nk_boxed((L), #T, sizeof(T))
	#define lua_push_nk_boxed(L, ptr, T)		_lua_push_nk_boxed((L), #T, ptr, sizeof(T))
	#define lua_check_nk_boxed(L, arg, T)		(T*)luaL_checkudata((L), (arg), #T)
	#define lua_check_nk_flags_boxed(L, arg)	lua_check_nk_boxed((L), (arg), nk_flags)
	#define lua_check_nk_uint_boxed(L, arg)		lua_check_nk_boxed((L), (arg), nk_uint)
	#define lua_check_nk_float_boxed(L, arg)	lua_check_nk_boxed((L), (arg), float)

	// struct boxed
	#define lua_new_nk_struct(L, T)				(struct T*)_lua_new_nk_boxed((L), #T, sizeof(struct T))
	#define lua_push_nk_struct(L, ptr, T)		_lua_push_nk_boxed((L), #T, ptr, sizeof(struct T))
	#define lua_check_nk_struct(L, arg, T)		(struct T*)luaL_checkudata((L), (arg), #T)
	#define lua_check_nk_vec2(L, arg)			lua_check_nk_struct((L), (arg), nk_vec2)
	#define lua_check_nk_rect(L, arg)			lua_check_nk_struct((L), (arg), nk_rect)
	#define lua_check_nk_color(L, arg)			lua_check_nk_struct((L), (arg), nk_color)
	#define lua_check_nk_image(L, arg)			lua_check_nk_struct((L), (arg), nk_image)
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
	#define lua_check_nk_buffer(L, arg)			lua_check_nk_struct((L), (arg), nk_buffer)
	#define lua_check_nk_input(L, arg)			lua_check_nk_struct((L), (arg), nk_input)
	#define lua_check_nk_command_buffer(L, arg)	lua_check_nk_struct((L), (arg), nk_command_buffer)

	static inline void lua_push_nk_vec2(lua_State* L, struct nk_vec2 v) { lua_push_nk_struct(L, &v, nk_vec2); }
	static inline void lua_push_nk_rect(lua_State* L, struct nk_rect v) { lua_push_nk_struct(L, &v, nk_rect); }
	static inline void lua_push_nk_color(lua_State* L, struct nk_color v) { lua_push_nk_struct(L, &v, nk_color); }
	static inline void lua_push_nk_image(lua_State* L, struct nk_image v) { lua_push_nk_struct(L, &v, nk_image); }
	static inline void lua_push_nk_style_item(lua_State* L, struct nk_style_item v) { lua_push_nk_struct(L, &v, nk_style_item); }

	// pointer boxed
	#define lua_new_nk_pointer(L, T)			(struct T**)_lua_new_nk_boxed((L), #T "*", sizeof(struct T**))
	#define lua_push_nk_pointer(L, ptr, T)		(*(lua_new_nk_pointer(L, T)) = ptr)
	#define lua_check_nk_pointer(L, arg, T)		(*(struct T**)luaL_checkudata((L), (arg), #T "*"))
	static struct nk_context* lua_check_nk_context(lua_State* L, int arg) { return lua_check_nk_pointer((L), (arg), nk_context); }
	static struct nk_font* lua_check_nk_font(lua_State* L, int arg) { return lua_check_nk_pointer((L), (arg), nk_font); }

	static inline void lua_push_nk_context_ptr(lua_State* L, struct nk_context* v) { lua_push_nk_pointer(L, v, nk_context); }
	static inline void lua_push_nk_window_ptr(lua_State* L, struct nk_window* v) { lua_push_nk_pointer(L, v, nk_window); }
	static inline void lua_push_nk_panel_ptr(lua_State* L, struct nk_panel* v) { lua_push_nk_pointer(L, v, nk_panel); }
	static inline void lua_push_nk_font_ptr(lua_State* L, struct nk_font* v) { lua_push_nk_pointer(L, v, nk_font); }

	// misc
	#define lua_check_nk_plugin_filter(L, arg)	(nk_plugin_filter)luaL_checkudata((L), (arg), "nk_plugin_filter")
	static inline void lua_push_nk_plugin_filter(lua_State* L, nk_plugin_filter v) { lua_push_nk_boxed(L, &v, nk_plugin_filter); }

	// array
	struct nk_char_array { int max; int len; char arr[1]; };
	struct nk_string_array { int len; const char* arr[1]; };
	struct nk_float_array { int len; float arr[1]; };
	struct nk_vec2_array { nk_uint len; struct nk_vec2 arr[1]; };

	#define lua_check_nk_char_array(L, arg)		lua_check_nk_struct((L), (arg), nk_char_array)
	#define lua_check_nk_string_array(L, arg)	lua_check_nk_struct((L), (arg), nk_string_array)

	static float* lua_check_nk_float_array_buffer(lua_State* L, int arg, int len) {
		struct nk_float_array* v = lua_check_nk_struct(L, arg, nk_float_array);
		luaL_argcheck(L, (len <= v->len), arg, "nk_float_array out of range");
		return v->arr;
	}

	static struct nk_vec2* lua_check_nk_vec2_array_buffer(lua_State* L, int arg, nk_uint len) {
		struct nk_vec2_array* v = lua_check_nk_struct(L, arg, nk_vec2_array);
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

#else
	#error "inner file, can NOT direct include"
#endif


// apis


#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API int nk_init_default(struct nk_context*, const struct nk_user_font*);
// LUA_NK_API( lua_pushboolean, nk_init_default, lua_check_nk_context(L,1), lua_check_nk_user_font(L,2) )
#endif

// NK_API int nk_init_fixed(struct nk_context*, void *memory, nk_size size, const struct nk_user_font*);
// NK_API int nk_init(struct nk_context*, struct nk_allocator*, const struct nk_user_font*);

// NK_API int nk_init_custom(struct nk_context*, struct nk_buffer *cmds, struct nk_buffer *pool, const struct nk_user_font*);
// LUA_NK_API( lua_pushboolean, nk_init_custom, lua_check_nk_context(L,1), lua_check_nk_buffer(L,2), lua_check_nk_buffer(L,3), lua_check_nk_user_font(L,4) )

// NK_API void nk_clear(struct nk_context*);
LUA_NK_API_NORET( nk_clear, lua_check_nk_context(L,1) )

// NK_API void nk_free(struct nk_context*);
// LUA_NK_API_NORET( nk_free, lua_check_nk_context(L,1) )

#ifdef NK_INCLUDE_COMMAND_USERDATA

// NK_API void nk_set_user_data(struct nk_context*, nk_handle handle);
// LUA_NK_API_NORET( nk_set_user_data, lua_check_nk_context(L,1), nk_handle_ptr(lua_topointer(L,2)) )

#endif


// NK_API void nk_input_begin(struct nk_context*);
LUA_NK_API_NORET( nk_input_begin, lua_check_nk_context(L,1) )

// NK_API void nk_input_motion(struct nk_context*, int x, int y);
LUA_NK_API_NORET( nk_input_motion, lua_check_nk_context(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3) )

// NK_API void nk_input_key(struct nk_context*, enum nk_keys, int down);
LUA_NK_API_NORET( nk_input_key, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_keys), lua_toboolean(L,3) )

// NK_API void nk_input_button(struct nk_context*, enum nk_buttons, int x, int y, int down);
LUA_NK_API_NORET( nk_input_button, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_buttons)
		, lua_check_nk_int(L,3), lua_check_nk_int(L,4)
		, lua_toboolean(L,5) )

// NK_API void nk_input_scroll(struct nk_context*, struct nk_vec2 val);
LUA_NK_API_NORET( nk_input_scroll, lua_check_nk_context(L,1), *lua_check_nk_vec2(L,2) )

// NK_API void nk_input_char(struct nk_context*, char);
LUA_NK_API_NORET( nk_input_char, lua_check_nk_context(L,1), lua_check_nk_char(L,2) )

// NK_API void nk_input_glyph(struct nk_context*, const nk_glyph);
LUA_NK_API_NORET( nk_input_glyph, lua_check_nk_context(L,1), lua_check_nk_glyph(L,2) )

// NK_API void nk_input_unicode(struct nk_context*, nk_rune);
LUA_NK_API_NORET( nk_input_unicode, lua_check_nk_context(L,1), (nk_rune)luaL_checkinteger(L,2) )

// NK_API void nk_input_end(struct nk_context*);
LUA_NK_API_NORET( nk_input_end, lua_check_nk_context(L,1) )

// NK_API const struct nk_command* nk__begin(struct nk_context*);
// LUA_NK_API( lua_push_nk_command_buffer, nk__begin, lua_check_nk_context(L,1) )

// NK_API const struct nk_command* nk__next(struct nk_context*, const struct nk_command*);
// LUA_NK_API( lua_push_nk_command_buffer, nk__next, lua_check_nk_context(L,1), lua_check_nk_command_buffer(L,2) )

// #define nk_foreach(c, ctx) for((c) = nk__begin(lua_check_nk_context(L,1) ); (c) != 0; (c) = nk__next(lua_check_nk_context(L,1),c) )
#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT

// NK_API nk_flags nk_convert(struct nk_context*, struct nk_buffer *cmds, struct nk_buffer *vertices, struct nk_buffer *elements, const struct nk_convert_config*);

// NK_API const struct nk_draw_command* nk__draw_begin(const struct nk_context*, const struct nk_buffer*);
// LUA_NK_API( lua_push_nk_command_buffer, nk__draw_begin, lua_check_nk_context(L,1), lua_check_nk_buffer(L,2) )

// NK_API const struct nk_draw_command* nk__draw_end(const struct nk_context*, const struct nk_buffer*);
// LUA_NK_API( lua_push_nk_command_buffer, nk__draw_end, lua_check_nk_context(L,1), lua_check_nk_buffer(L,2) )

// NK_API const struct nk_draw_command* nk__draw_next(const struct nk_draw_command*, const struct nk_buffer*, const struct nk_context*);

// #define nk_draw_foreach(cmd,ctx, b) for((cmd)=nk__draw_begin(lua_check_nk_context(L,1), b); (cmd)!=0; (cmd)=nk__draw_next(cmd, b, ctx) )
#endif

// NK_API int nk_begin(struct nk_context *ctx, const char *title, struct nk_rect bounds, nk_flags flags);
LUA_NK_API( lua_pushboolean, nk_begin, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_rect(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_begin_titled(struct nk_context *ctx, const char *name, const char *title, struct nk_rect bounds, nk_flags flags);
LUA_NK_API( lua_pushboolean, nk_begin_titled, lua_check_nk_context(L,1), luaL_checkstring(L,2), luaL_checkstring(L,3), *lua_check_nk_rect(L,4), lua_check_nk_flags(L,5) )

// NK_API void nk_end(struct nk_context *ctx);
LUA_NK_API_NORET( nk_end, lua_check_nk_context(L,1) )

// NK_API struct nk_window *nk_window_find(struct nk_context *ctx, const char *name);
LUA_NK_API( lua_push_nk_window_ptr, nk_window_find, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API struct nk_rect nk_window_get_bounds(const struct nk_context *ctx)
LUA_NK_API( lua_push_nk_rect, nk_window_get_bounds, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_window_get_position(const struct nk_context *ctx);
LUA_NK_API( lua_push_nk_vec2, nk_window_get_position, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_window_get_size(const struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_window_get_size, lua_check_nk_context(L,1) )

// NK_API float nk_window_get_width(const struct nk_context*);
LUA_NK_API( lua_pushnumber, nk_window_get_width, lua_check_nk_context(L,1) )

// NK_API float nk_window_get_height(const struct nk_context*);
LUA_NK_API( lua_pushnumber, nk_window_get_height, lua_check_nk_context(L,1) )

// NK_API struct nk_panel* nk_window_get_panel(struct nk_context*);
LUA_NK_API( lua_push_nk_panel_ptr, nk_window_get_panel, lua_check_nk_context(L,1) )

// NK_API struct nk_rect nk_window_get_content_region(struct nk_context*);
LUA_NK_API( lua_push_nk_rect, nk_window_get_content_region, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_window_get_content_region_min(struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_window_get_content_region_min, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_window_get_content_region_max(struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_window_get_content_region_max, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_window_get_content_region_size(struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_window_get_content_region_size, lua_check_nk_context(L,1) )

// NK_API struct nk_command_buffer* nk_window_get_canvas(struct nk_context*);
// LUA_NK_API( lua_push_nk_command_buffer, nk_window_get_canvas, lua_check_nk_context(L,1) )

// NK_API int nk_window_has_focus(const struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_window_has_focus, lua_check_nk_context(L,1) )

// NK_API int nk_window_is_collapsed(struct nk_context *ctx, const char *name);
LUA_NK_API( lua_pushboolean, nk_window_is_collapsed, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_window_is_closed(struct nk_context*, const char*);
LUA_NK_API( lua_pushboolean, nk_window_is_closed, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_window_is_hidden(struct nk_context*, const char*);
LUA_NK_API( lua_pushboolean, nk_window_is_hidden, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_window_is_active(struct nk_context*, const char*);
LUA_NK_API( lua_pushboolean, nk_window_is_active, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_window_is_hovered(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_window_is_hovered, lua_check_nk_context(L,1) )

// NK_API int nk_window_is_any_hovered(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_window_is_any_hovered, lua_check_nk_context(L,1) )

// NK_API int nk_item_is_any_active(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_item_is_any_active, lua_check_nk_context(L,1) )

// NK_API void nk_window_set_bounds(struct nk_context*, const char *name, struct nk_rect bounds);
LUA_NK_API_NORET( nk_window_set_bounds, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_rect(L,3) )

// NK_API void nk_window_set_position(struct nk_context*, const char *name, struct nk_vec2 pos);
LUA_NK_API_NORET( nk_window_set_position, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_vec2(L,3) )

// NK_API void nk_window_set_size(struct nk_context*, const char *name, struct nk_vec2);
LUA_NK_API_NORET( nk_window_set_size, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_vec2(L,3) )

// NK_API void nk_window_set_focus(struct nk_context*, const char *name);
LUA_NK_API_NORET( nk_window_set_focus, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API void nk_window_close(struct nk_context *ctx, const char *name);
LUA_NK_API_NORET( nk_window_close, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API void nk_window_collapse(struct nk_context*, const char *name, enum nk_collapse_states state);
LUA_NK_API_NORET( nk_window_collapse, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_enum(L,3,nk_collapse_states) )

// NK_API void nk_window_collapse_if(struct nk_context*, const char *name, enum nk_collapse_states, int cond);

// NK_API void nk_window_show(struct nk_context*, const char *name, enum nk_show_states);
LUA_NK_API_NORET( nk_window_show, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_enum(L,3,nk_show_states) )

// NK_API void nk_window_show_if(struct nk_context*, const char *name, enum nk_show_states, int cond);

// NK_API void nk_layout_set_min_row_height(struct nk_context*, float height);
LUA_NK_API_NORET( nk_layout_set_min_row_height, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_reset_min_row_height(struct nk_context*);
LUA_NK_API_NORET( nk_layout_reset_min_row_height, lua_check_nk_context(L,1) )

// NK_API struct nk_rect nk_layout_widget_bounds(struct nk_context*);
LUA_NK_API( lua_push_nk_rect, nk_layout_widget_bounds, lua_check_nk_context(L,1) )

// NK_API float nk_layout_ratio_from_pixel(struct nk_context*, float pixel_width);
LUA_NK_API( lua_pushnumber, nk_layout_ratio_from_pixel, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_row_dynamic(struct nk_context *ctx, float height, int cols);
LUA_NK_API_NORET( nk_layout_row_dynamic, lua_check_nk_context(L,1), lua_check_nk_float(L,2), luaL_checkinteger(L,3) )

// NK_API void nk_layout_row_static(struct nk_context *ctx, float height, int item_width, int cols);
LUA_NK_API_NORET( nk_layout_row_static, lua_check_nk_context(L,1), lua_check_nk_float(L,2), luaL_checkinteger(L,3), luaL_checkinteger(L,4) )

// NK_API void nk_layout_row_begin(struct nk_context *ctx, enum nk_layout_format fmt, float row_height, int cols);
LUA_NK_API_NORET( nk_layout_row_begin, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_layout_format), lua_check_nk_float(L,3), luaL_checkinteger(L,4) )

// NK_API void nk_layout_row_push(struct nk_context*, float value);
LUA_NK_API_NORET( nk_layout_row_push, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_row_end(struct nk_context*);
LUA_NK_API_NORET( nk_layout_row_end, lua_check_nk_context(L,1) )

// NK_API void nk_layout_row(struct nk_context*, enum nk_layout_format, float height, int cols, const float *ratio);
LUA_NK_API_NORET( nk_layout_row, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_layout_format), lua_check_nk_float(L,3)
		, lua_check_nk_int(L,4), lua_check_nk_float_array_buffer(L,5,lua_check_nk_int(L,4)) )

// NK_API void nk_layout_row_template_begin(struct nk_context*, float row_height);
LUA_NK_API_NORET( nk_layout_row_template_begin, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_row_template_push_dynamic(struct nk_context*);
LUA_NK_API_NORET( nk_layout_row_template_push_dynamic, lua_check_nk_context(L,1) )

// NK_API void nk_layout_row_template_push_variable(struct nk_context*, float min_width);
LUA_NK_API_NORET( nk_layout_row_template_push_variable, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_row_template_push_static(struct nk_context*, float width);
LUA_NK_API_NORET( nk_layout_row_template_push_static, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_layout_row_template_end(struct nk_context*);
LUA_NK_API_NORET( nk_layout_row_template_end, lua_check_nk_context(L,1) )

// NK_API void nk_layout_space_begin(struct nk_context*, enum nk_layout_format, float height, int widget_count);
LUA_NK_API_NORET( nk_layout_space_begin, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_layout_format), lua_check_nk_float(L,3), lua_check_nk_int(L,4) )

// NK_API void nk_layout_space_push(struct nk_context*, struct nk_rect);
LUA_NK_API_NORET( nk_layout_space_push, lua_check_nk_context(L,1), *lua_check_nk_rect(L,2) )

// NK_API void nk_layout_space_end(struct nk_context*);
LUA_NK_API_NORET( nk_layout_space_end, lua_check_nk_context(L,1) )

// NK_API struct nk_rect nk_layout_space_bounds(struct nk_context*);
LUA_NK_API( lua_push_nk_rect, nk_layout_space_bounds, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_layout_space_to_screen(struct nk_context*, struct nk_vec2);
LUA_NK_API( lua_push_nk_vec2, nk_layout_space_to_screen, lua_check_nk_context(L,1), *lua_check_nk_vec2(L,2) )

// NK_API struct nk_vec2 nk_layout_space_to_local(struct nk_context*, struct nk_vec2);
LUA_NK_API( lua_push_nk_vec2, nk_layout_space_to_local, lua_check_nk_context(L,1), *lua_check_nk_vec2(L,2) )

// NK_API struct nk_rect nk_layout_space_rect_to_screen(struct nk_context*, struct nk_rect);
LUA_NK_API( lua_push_nk_rect, nk_layout_space_rect_to_screen, lua_check_nk_context(L,1), *lua_check_nk_rect(L,2) )

// NK_API struct nk_rect nk_layout_space_rect_to_local(struct nk_context*, struct nk_rect);
LUA_NK_API( lua_push_nk_rect, nk_layout_space_rect_to_local, lua_check_nk_context(L,1), *lua_check_nk_rect(L,2) )

// NK_API int nk_group_begin(struct nk_context*, const char *title, nk_flags);
LUA_NK_API( lua_pushboolean, nk_group_begin, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3) )

// NK_API int nk_group_scrolled_offset_begin(struct nk_context*, nk_uint *x_offset, nk_uint *y_offset, const char*, nk_flags);
LUA_NK_API( lua_pushboolean, nk_group_scrolled_offset_begin, lua_check_nk_context(L,1), lua_check_nk_uint_boxed(L,2)
		, lua_check_nk_uint_boxed(L,3), luaL_checkstring(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_group_scrolled_begin(struct nk_context*, struct nk_scroll*, const char *title, nk_flags);
LUA_NK_API( lua_pushboolean, nk_group_scrolled_begin, lua_check_nk_context(L,1), lua_check_nk_scroll(L,2)
		, luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API void nk_group_scrolled_end(struct nk_context*);
LUA_NK_API_NORET( nk_group_scrolled_end, lua_check_nk_context(L,1) )

// NK_API void nk_group_end(struct nk_context*);
LUA_NK_API_NORET( nk_group_end, lua_check_nk_context(L,1) )

// NK_API int nk_list_view_begin(struct nk_context*, struct nk_list_view *out, const char *id, nk_flags, int row_height, int row_count);
LUA_NK_API( lua_pushboolean, nk_list_view_begin, lua_check_nk_context(L,1), lua_check_nk_list_view(L,2)
		, luaL_checkstring(L,3), lua_check_nk_flags(L,4)
		, lua_check_nk_int(L,5), lua_check_nk_int(L,6) )

// NK_API void nk_list_view_end(struct nk_list_view*);
LUA_NK_API_NORET( nk_list_view_end, lua_check_nk_list_view(L,1) )

// #define nk_tree_push(lua_check_nk_context(L,1), type, title, state) nk_tree_push_hashed(lua_check_nk_context(L,1), type, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
// #define nk_tree_push_id(lua_check_nk_context(L,1), type, title, state, id) nk_tree_push_hashed(lua_check_nk_context(L,1), type, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)

// NK_API int nk_tree_push_hashed(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states initial_state, const char *hash, int len,int seed);
LUA_NK_API( lua_pushboolean, nk_tree_push_hashed, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_tree_type), luaL_checkstring(L,3)
		, lua_check_nk_enum(L,4,nk_collapse_states)
		, luaL_checkstring(L,5), lua_check_nk_text_len(L,6)
		, lua_check_nk_int(L,7) )

// #define nk_tree_image_push(ctx, type, img, title, state) nk_tree_image_push_hashed(ctx, type, img, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),__LINE__)
// #define nk_tree_image_push_id(ctx, type, img, title, state, id) nk_tree_image_push_hashed(ctx, type, img, title, state, NK_FILE_LINE,nk_strlen(NK_FILE_LINE),id)

// NK_API int nk_tree_image_push_hashed(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states initial_state, const char *hash, int len,int seed);
LUA_NK_API( lua_pushboolean, nk_tree_image_push_hashed, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_tree_type)
	, *lua_check_nk_image(L,3), luaL_checkstring(L,4), lua_check_nk_enum(L,5,nk_collapse_states)
	, luaL_checkstring(L,6), lua_check_nk_text_len(L,7), lua_check_nk_int(L,8) )

// NK_API void nk_tree_pop(struct nk_context*);
LUA_NK_API_NORET( nk_tree_pop, lua_check_nk_context(L,1) )

// NK_API int nk_tree_state_push(struct nk_context*, enum nk_tree_type, const char *title, enum nk_collapse_states *state);
LUA_NK_API_DEFINE( nk_tree_state_push, {
	enum nk_collapse_states state = lua_check_nk_enum(L,4,nk_collapse_states);
	lua_pushboolean(L, nk_tree_state_push(lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_tree_type), luaL_checkstring(L,3), &state));
	lua_pushinteger(L, state);
	return 2;
} )

// NK_API int nk_tree_state_image_push(struct nk_context*, enum nk_tree_type, struct nk_image, const char *title, enum nk_collapse_states *state);
LUA_NK_API_DEFINE( nk_tree_state_image_push, {
	enum nk_collapse_states state = lua_check_nk_enum(L,4,nk_collapse_states);
	lua_pushboolean(L, nk_tree_state_image_push(lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_tree_type), *lua_check_nk_image(L,3), luaL_checkstring(L,4), &state));
	lua_pushinteger(L, state);
	return 2;
} )

// NK_API void nk_tree_state_pop(struct nk_context*);
LUA_NK_API_NORET( nk_tree_state_pop, lua_check_nk_context(L,1) )

// NK_API enum nk_widget_layout_states nk_widget(struct nk_rect*, const struct nk_context*);
LUA_NK_API( lua_pushinteger, nk_widget, lua_check_nk_rect(L,1), lua_check_nk_context(L,2) )

// NK_API enum nk_widget_layout_states nk_widget_fitting(struct nk_rect*, struct nk_context*, struct nk_vec2);
LUA_NK_API( lua_pushinteger, nk_widget_fitting, lua_check_nk_rect(L,1), lua_check_nk_context(L,2), *lua_check_nk_vec2(L,3) )

// NK_API struct nk_rect nk_widget_bounds(struct nk_context*);
LUA_NK_API( lua_push_nk_rect, nk_widget_bounds, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_widget_position(struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_widget_position, lua_check_nk_context(L,1) )

// NK_API struct nk_vec2 nk_widget_size(struct nk_context*);
LUA_NK_API( lua_push_nk_vec2, nk_widget_size, lua_check_nk_context(L,1) )

// NK_API float nk_widget_width(struct nk_context*);
LUA_NK_API( lua_pushnumber, nk_widget_width, lua_check_nk_context(L,1) )

// NK_API float nk_widget_height(struct nk_context*);
LUA_NK_API( lua_pushnumber, nk_widget_height, lua_check_nk_context(L,1) )

// NK_API int nk_widget_is_hovered(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_widget_is_hovered, lua_check_nk_context(L,1) )

// NK_API int nk_widget_is_mouse_clicked(struct nk_context*, enum nk_buttons);
LUA_NK_API( lua_pushboolean, nk_widget_is_mouse_clicked, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_buttons) )

// NK_API int nk_widget_has_mouse_click_down(struct nk_context*, enum nk_buttons, int down);
LUA_NK_API( lua_pushboolean, nk_widget_has_mouse_click_down, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_buttons), lua_toboolean(L,3) )

// NK_API void nk_spacing(struct nk_context*, int cols);
LUA_NK_API_NORET( nk_spacing, lua_check_nk_context(L,1), lua_check_nk_int(L,2) )

// NK_API void nk_text(struct nk_context*, const char*, int, nk_flags);
LUA_NK_API_NORET( nk_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3), lua_check_nk_flags(L,4) )

// NK_API void nk_text_colored(struct nk_context*, const char*, int, nk_flags, struct nk_color);
LUA_NK_API_NORET( nk_text_colored, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3), lua_check_nk_flags(L,4), *lua_check_nk_color(L,5) )

// NK_API void nk_text_wrap(struct nk_context*, const char*, int);
LUA_NK_API_NORET( nk_text_wrap, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3) )

// NK_API void nk_text_wrap_colored(struct nk_context*, const char*, int, struct nk_color);
LUA_NK_API_NORET( nk_text_wrap_colored, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3), *lua_check_nk_color(L,4) )

// NK_API void nk_label(struct nk_context*, const char*, nk_flags align);
LUA_NK_API_NORET( nk_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3) )

// NK_API void nk_label_colored(struct nk_context*, const char*, nk_flags align, struct nk_color);
LUA_NK_API_NORET( nk_label_colored, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), *lua_check_nk_color(L,4) )

// NK_API void nk_label_wrap(struct nk_context*, const char*);
LUA_NK_API_NORET( nk_label_wrap, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API void nk_label_colored_wrap(struct nk_context*, const char*, struct nk_color);
LUA_NK_API_NORET( nk_label_colored_wrap, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_color(L,3) )

// NK_API void nk_image(struct nk_context*, struct nk_image);
LUA_NK_API_NORET( nk_image, lua_check_nk_context(L,1), *lua_check_nk_image(L,2) )

#ifdef NK_INCLUDE_STANDARD_VARARGS
// NK_API void nk_labelf(struct nk_context*, nk_flags, const char*, ...);
// NK_API void nk_labelf_colored(struct nk_context*, nk_flags align, struct nk_color, const char*,...);
// NK_API void nk_labelf_wrap(struct nk_context*, const char*,...);
// NK_API void nk_labelf_colored_wrap(struct nk_context*, struct nk_color, const char*,...);

// NK_API void nk_value_bool(struct nk_context*, const char *prefix, int);
LUA_NK_API_NORET( nk_value_bool, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_toboolean(L,3) )

// NK_API void nk_value_int(struct nk_context*, const char *prefix, int);
LUA_NK_API_NORET( nk_value_int, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3) )

// NK_API void nk_value_uint(struct nk_context*, const char *prefix, unsigned int);
LUA_NK_API_NORET( nk_value_uint, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_uint(L,3) )

// NK_API void nk_value_float(struct nk_context*, const char *prefix, float);
LUA_NK_API_NORET( nk_value_float, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_float(L,3) )

// NK_API void nk_value_color_byte(struct nk_context*, const char *prefix, struct nk_color);
LUA_NK_API_NORET( nk_value_color_byte, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_color(L,3) )

// NK_API void nk_value_color_float(struct nk_context*, const char *prefix, struct nk_color);
LUA_NK_API_NORET( nk_value_color_float, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_color(L,3) )

// NK_API void nk_value_color_hex(struct nk_context*, const char *prefix, struct nk_color);
LUA_NK_API_NORET( nk_value_color_hex, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_color(L,3) )
#endif

// NK_API int nk_button_text(struct nk_context*, const char *title, int len);
LUA_NK_API( lua_pushboolean, nk_button_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3) )

// NK_API int nk_button_label(struct nk_context*, const char *title);
LUA_NK_API( lua_pushboolean, nk_button_label, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_button_color(struct nk_context*, struct nk_color);
LUA_NK_API( lua_pushboolean, nk_button_color, lua_check_nk_context(L,1), *lua_check_nk_color(L,2) )

// NK_API int nk_button_symbol(struct nk_context*, enum nk_symbol_type);
LUA_NK_API( lua_pushboolean, nk_button_symbol, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type) )

// NK_API int nk_button_image(struct nk_context*, struct nk_image img);
LUA_NK_API( lua_pushboolean, nk_button_image, lua_check_nk_context(L,1), *lua_check_nk_image(L,2) )

// NK_API int nk_button_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags text_alignment);
LUA_NK_API( lua_pushboolean, nk_button_symbol_label, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_button_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_button_symbol_text, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type)
	, luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_button_image_label(struct nk_context*, struct nk_image img, const char*, nk_flags text_alignment);
LUA_NK_API( lua_pushboolean, nk_button_image_label, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_button_image_text(struct nk_context*, struct nk_image img, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_button_image_text, lua_check_nk_context(L,1), *lua_check_nk_image(L,2)
		, luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_button_text_styled(struct nk_context*, const struct nk_style_button*, const char *title, int len);
LUA_NK_API( lua_pushboolean, nk_button_text_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2)
		, luaL_checkstring(L,3), lua_check_nk_text_len(L,4) )

// NK_API int nk_button_label_styled(struct nk_context*, const struct nk_style_button*, const char *title);
LUA_NK_API( lua_pushboolean, nk_button_label_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2), luaL_checkstring(L,3) )

// NK_API int nk_button_symbol_styled(struct nk_context*, const struct nk_style_button*, enum nk_symbol_type);
LUA_NK_API( lua_pushboolean, nk_button_symbol_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2), lua_check_nk_enum(L,3,nk_symbol_type) )

// NK_API int nk_button_image_styled(struct nk_context*, const struct nk_style_button*, struct nk_image img);
LUA_NK_API( lua_pushboolean, nk_button_image_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2), *lua_check_nk_image(L,3) )

// NK_API int nk_button_symbol_text_styled(struct nk_context*,const struct nk_style_button*, enum nk_symbol_type, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_button_symbol_text_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2), lua_check_nk_enum(L,3,nk_symbol_type)
		, luaL_checkstring(L,4), lua_check_nk_text_len(L,5), lua_check_nk_flags(L,6) )

// NK_API int nk_button_symbol_label_styled(struct nk_context *ctx, const struct nk_style_button *style, enum nk_symbol_type symbol, const char *title, nk_flags align);
LUA_NK_API( lua_pushboolean, nk_button_symbol_label_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2)
	, lua_check_nk_enum(L,3,nk_symbol_type), luaL_checkstring(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_button_image_label_styled(struct nk_context*,const struct nk_style_button*, struct nk_image img, const char*, nk_flags text_alignment);
LUA_NK_API( lua_pushboolean, nk_button_image_label_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2)
	, *lua_check_nk_image(L,3), luaL_checkstring(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_button_image_text_styled(struct nk_context*,const struct nk_style_button*, struct nk_image img, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_button_image_text_styled, lua_check_nk_context(L,1), lua_check_nk_style_button(L,2), *lua_check_nk_image(L,3)
		, luaL_checkstring(L,4), lua_check_nk_text_len(L,5), lua_check_nk_flags(L,6) )

// NK_API void nk_button_set_behavior(struct nk_context*, enum nk_button_behavior);
LUA_NK_API_NORET( nk_button_set_behavior, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_button_behavior) )

// NK_API int nk_button_push_behavior(struct nk_context*, enum nk_button_behavior);
LUA_NK_API( lua_pushboolean, nk_button_push_behavior, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_button_behavior) )

// NK_API int nk_button_pop_behavior(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_button_pop_behavior, lua_check_nk_context(L,1) )

// NK_API int nk_check_label(struct nk_context*, const char*, int active);
LUA_NK_API( lua_pushboolean, nk_check_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_toboolean(L,3) )

// NK_API int nk_check_text(struct nk_context*, const char*, int,int active);
LUA_NK_API( lua_pushboolean, nk_check_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_toboolean(L,4) )

// NK_API unsigned nk_check_flags_label(struct nk_context*, const char*, unsigned int flags, unsigned int value);
LUA_NK_API( lua_pushinteger, nk_check_flags_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), lua_check_nk_uint(L,4) )

// NK_API unsigned nk_check_flags_text(struct nk_context*, const char*, int, unsigned int flags, unsigned int value);
LUA_NK_API( lua_pushinteger, nk_check_flags_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3)
	, lua_check_nk_flags(L,4), lua_check_nk_uint(L,5) )

// NK_API int nk_checkbox_label(struct nk_context*, const char*, int *active);
LUA_NK_API_DEFINE( nk_checkbox_label, {
	int active = lua_toboolean(L,3);
	lua_pushboolean(L, nk_checkbox_label(lua_check_nk_context(L,1), luaL_checkstring(L,2), &active));
	lua_pushboolean(L, active);
	return 2;
} )

// NK_API int nk_checkbox_text(struct nk_context*, const char*, int, int *active);
LUA_NK_API_DEFINE( nk_checkbox_text, {
	int active = lua_toboolean(L,3);
	lua_pushboolean(L, nk_checkbox_text(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), &active));
	lua_pushboolean(L, active);
	return 2;
} )

// NK_API int nk_checkbox_flags_label(struct nk_context*, const char*, unsigned int *flags, unsigned int value);
LUA_NK_API_DEFINE( nk_checkbox_flags_label, {
	unsigned int flags = lua_check_nk_flags(L,3);
	lua_pushboolean(L, nk_checkbox_flags_label(lua_check_nk_context(L,1), luaL_checkstring(L,2), &flags, lua_check_nk_uint(L,4)));
	lua_pushinteger(L, flags);
	return 2;
} )

// NK_API int nk_checkbox_flags_text(struct nk_context*, const char*, int, unsigned int *flags, unsigned int value);
LUA_NK_API_DEFINE( nk_checkbox_flags_text, {
	unsigned int flags = lua_check_nk_flags(L,4);
	lua_pushboolean(L, nk_checkbox_flags_text(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), &flags, lua_check_nk_uint(L,5)));
	lua_pushinteger(L, flags);
	return 2;
} )

// NK_API int nk_radio_label(struct nk_context*, const char*, int *active);
LUA_NK_API_DEFINE( nk_radio_label, {
	int active = lua_toboolean(L,3);
	lua_pushboolean(L, nk_radio_label(lua_check_nk_context(L,1), luaL_checkstring(L,2), &active));
	lua_pushboolean(L, active);
	return 2;
} )

// NK_API int nk_radio_text(struct nk_context*, const char*, int, int *active);
LUA_NK_API_DEFINE( nk_radio_text, {
	int active = lua_toboolean(L,4);
	lua_pushboolean(L, nk_radio_text(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), &active));
	lua_pushboolean(L, active);
	return 2;
} )

// NK_API int nk_option_label(struct nk_context*, const char*, int active);
LUA_NK_API( lua_pushboolean, nk_option_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_toboolean(L,3) )

// NK_API int nk_option_text(struct nk_context*, const char*, int, int active);
LUA_NK_API( lua_pushboolean, nk_option_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_toboolean(L,4) )

// NK_API int nk_selectable_label(struct nk_context*, const char*, nk_flags align, int *value);
LUA_NK_API_DEFINE( nk_selectable_label, {
	int value = lua_check_nk_int(L,4);
	lua_pushboolean(L, nk_selectable_label(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), &value));
	lua_pushinteger(L, value);
	return 2;
} )

// NK_API int nk_selectable_text(struct nk_context*, const char*, int, nk_flags align, int *value);
LUA_NK_API_DEFINE( nk_selectable_text, {
	int value = lua_check_nk_int(L,5);
	lua_pushboolean(L, nk_selectable_text(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4), &value));
	lua_pushinteger(L, value);
	return 2;
} )

// NK_API int nk_selectable_image_label(struct nk_context*,struct nk_image,  const char*, nk_flags align, int *value);
LUA_NK_API_DEFINE( nk_selectable_image_label, {
	int value = lua_check_nk_int(L,5);
	lua_pushboolean(L, nk_selectable_image_label(lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4), &value));
	lua_pushinteger(L, value);
	return 2;
} )

// NK_API int nk_selectable_image_text(struct nk_context*,struct nk_image, const char*, int, nk_flags align, int *value);
LUA_NK_API_DEFINE( nk_selectable_image_text, {
	int value = lua_check_nk_int(L,6);
	lua_pushboolean(L, nk_selectable_image_text(lua_check_nk_context(L,1), *lua_check_nk_image(L,2)
		, luaL_checkstring(L,3), lua_check_nk_text_len(L,4)
		, lua_check_nk_flags(L,5), &value));
	lua_pushinteger(L, value);
	return 2;
} )

// NK_API int nk_select_label(struct nk_context*, const char*, nk_flags align, int value);
LUA_NK_API( lua_pushboolean, nk_select_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), lua_check_nk_int(L,4) )

// NK_API int nk_select_text(struct nk_context*, const char*, int, nk_flags align, int value);
LUA_NK_API( lua_pushboolean, nk_select_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4), lua_check_nk_int(L,5) )

// NK_API int nk_select_image_label(struct nk_context*, struct nk_image,const char*, nk_flags align, int value);
LUA_NK_API( lua_pushboolean, nk_select_image_label, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4), lua_check_nk_int(L,5) )

// NK_API int nk_select_image_text(struct nk_context*, struct nk_image,const char*, int, nk_flags align, int value);
LUA_NK_API( lua_pushboolean, nk_select_image_text, lua_check_nk_context(L,1), *lua_check_nk_image(L,2)
	, luaL_checkstring(L,3), lua_check_nk_text_len(L,4)
	, lua_check_nk_flags(L,5), lua_check_nk_int(L,6) )

// NK_API float nk_slide_float(struct nk_context*, float min, float val, float max, float step);
LUA_NK_API( lua_pushnumber, nk_slide_float, lua_check_nk_context(L,1), lua_check_nk_float(L,2)
	, lua_check_nk_float(L,3), lua_check_nk_float(L,4), lua_check_nk_float(L,5) )

// NK_API int nk_slide_int(struct nk_context*, int min, int val, int max, int step);
LUA_NK_API( lua_pushinteger, nk_slide_int, lua_check_nk_context(L,1), lua_check_nk_int(L,2)
	, lua_check_nk_int(L,3), lua_check_nk_int(L,4), lua_check_nk_int(L,5) )

// NK_API int nk_slider_float(struct nk_context*, float min, float *val, float max, float step);
LUA_NK_API_DEFINE( nk_slider_float, {
	float val = lua_check_nk_float(L,3);
	lua_pushboolean(L, nk_slider_float(lua_check_nk_context(L,1), lua_check_nk_float(L,2), &val, lua_check_nk_float(L,4), lua_check_nk_float(L,5)));
	lua_pushnumber(L, val);
	return 2;
} )

// NK_API int nk_slider_int(struct nk_context*, int min, int *val, int max, int step);
LUA_NK_API_DEFINE( nk_slider_int, {
	int val = lua_check_nk_int(L,3);
	lua_pushboolean(L, nk_slider_int(lua_check_nk_context(L,1), lua_check_nk_int(L,2), &val, lua_check_nk_int(L,4), lua_check_nk_int(L,5)));
	lua_pushinteger(L, val);
	return 2;
} )

// NK_API int nk_progress(struct nk_context*, nk_size *cur, nk_size max, int modifyable);
LUA_NK_API_DEFINE( nk_progress, {
	nk_size cur = lua_check_nk_size(L,2);
	lua_pushboolean(L, nk_progress(lua_check_nk_context(L,1), &cur, lua_check_nk_size(L,3), lua_toboolean(L,4)));
	lua_pushinteger(L, cur);
	return 2;
} )

// NK_API nk_size nk_prog(struct nk_context*, nk_size cur, nk_size max, int modifyable);
LUA_NK_API( lua_pushinteger, nk_prog, lua_check_nk_context(L,1), lua_check_nk_size(L,2), lua_check_nk_size(L,3), lua_toboolean(L,4) )

// NK_API struct nk_color nk_color_picker(struct nk_context*, struct nk_color, enum nk_color_format);
LUA_NK_API( lua_push_nk_color, nk_color_picker, lua_check_nk_context(L,1), *lua_check_nk_color(L,2), lua_check_nk_enum(L,3,nk_color_format) )

// NK_API int nk_color_pick(struct nk_context*, struct nk_color*, enum nk_color_format);
LUA_NK_API( lua_pushboolean, nk_color_pick, lua_check_nk_context(L,1), lua_check_nk_color(L,2), lua_check_nk_enum(L,3,nk_color_format) )

// NK_API void nk_property_int(struct nk_context*, const char *name, int min, int *val, int max, int step, float inc_per_pixel);
LUA_NK_API_DEFINE( nk_property_int, {
	int val = lua_check_nk_int(L,4);
	nk_property_int(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3), &val
		, lua_check_nk_int(L,5), lua_check_nk_int(L,6), lua_check_nk_float(L,7));
	return 1;
} )

// NK_API void nk_property_float(struct nk_context*, const char *name, float min, float *val, float max, float step, float inc_per_pixel);
LUA_NK_API_DEFINE( nk_property_float, {
	float val = lua_check_nk_float(L,4);
	nk_property_float(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_float(L,3), &val
		, lua_check_nk_float(L,5), lua_check_nk_float(L,6), lua_check_nk_float(L,7));
	return 1;
} )

// NK_API void nk_property_double(struct nk_context*, const char *name, double min, double *val, double max, double step, float inc_per_pixel);
LUA_NK_API_DEFINE( nk_property_double, {
	double val = lua_check_nk_double(L,4);
	nk_property_double(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_double(L,3), &val
		, lua_check_nk_double(L,5), lua_check_nk_double(L,6), lua_check_nk_float(L,7));
	return 1;
} )

// NK_API int nk_propertyi(struct nk_context*, const char *name, int min, int val, int max, int step, float inc_per_pixel);
LUA_NK_API( lua_pushinteger, nk_propertyi, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3)
	, lua_check_nk_int(L,4), lua_check_nk_int(L,5), lua_check_nk_int(L,6), lua_check_nk_float(L,7) )

// NK_API float nk_propertyf(struct nk_context*, const char *name, float min, float val, float max, float step, float inc_per_pixel);
LUA_NK_API( lua_pushnumber, nk_propertyf, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6), lua_check_nk_float(L,7) )

// NK_API double nk_propertyd(struct nk_context*, const char *name, double min, double val, double max, double step, float inc_per_pixel);
LUA_NK_API( lua_pushnumber, nk_propertyd, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_double(L,3)
	, lua_check_nk_double(L,4), lua_check_nk_double(L,5), lua_check_nk_double(L,6), lua_check_nk_float(L,7) )

// NK_API nk_flags nk_edit_string(struct nk_context*, nk_flags, char *buffer, int *len, int max, nk_plugin_filter);
LUA_NK_API_DEFINE( nk_edit_string, {
	struct nk_char_array* b = lua_check_nk_char_array(L,3);
	lua_pushinteger(L, nk_edit_string(lua_check_nk_context(L,1), lua_check_nk_flags(L,2)
		, b->arr, &(b->len), b->max, lua_check_nk_plugin_filter(L,4)));
	return 1;
} )

// NK_API nk_flags nk_edit_string_zero_terminated(struct nk_context*, nk_flags, char *buffer, int max, NULL);

// NK_API nk_flags nk_edit_buffer(struct nk_context*, nk_flags, struct nk_text_edit*, nk_plugin_filter);
LUA_NK_API( lua_pushinteger, nk_edit_buffer, lua_check_nk_context(L,1), lua_check_nk_flags(L,2), lua_check_nk_text_edit(L,3), lua_check_nk_plugin_filter(L,4) )

// NK_API void nk_edit_focus(struct nk_context*, nk_flags flags);
LUA_NK_API_NORET( nk_edit_focus, lua_check_nk_context(L,1), lua_check_nk_flags(L,2) )

// NK_API void nk_edit_unfocus(struct nk_context*);
LUA_NK_API_NORET( nk_edit_unfocus, lua_check_nk_context(L,1) )

// NK_API int nk_chart_begin(struct nk_context*, enum nk_chart_type, int num, float min, float max);
LUA_NK_API( lua_pushboolean, nk_chart_begin, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type), lua_check_nk_int(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5) )

// NK_API int nk_chart_begin_colored(struct nk_context*, enum nk_chart_type, struct nk_color, struct nk_color active, int num, float min, float max);
LUA_NK_API( lua_pushboolean, nk_chart_begin_colored, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type), *lua_check_nk_color(L,3)
	, *lua_check_nk_color(L,4), lua_check_nk_int(L,5), lua_check_nk_float(L,6), lua_check_nk_float(L,7) )

// NK_API void nk_chart_add_slot(struct nk_context *ctx, const enum nk_chart_type, int count, float min_value, float max_value);
LUA_NK_API_NORET( nk_chart_add_slot, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type), lua_check_nk_int(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5) )

// NK_API void nk_chart_add_slot_colored(struct nk_context *ctx, const enum nk_chart_type, struct nk_color, struct nk_color active, int count, float min_value, float max_value);
LUA_NK_API_NORET( nk_chart_add_slot_colored, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type), *lua_check_nk_color(L,3)
	, *lua_check_nk_color(L,4), lua_check_nk_int(L,5), lua_check_nk_float(L,6), lua_check_nk_float(L,7) )

// NK_API nk_flags nk_chart_push(struct nk_context*, float);
LUA_NK_API( lua_pushinteger, nk_chart_push, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API nk_flags nk_chart_push_slot(struct nk_context*, float, int);
LUA_NK_API( lua_pushinteger, nk_chart_push_slot, lua_check_nk_context(L,1), lua_check_nk_float(L,2), lua_check_nk_int(L,3) )

// NK_API void nk_chart_end(struct nk_context*);
LUA_NK_API_NORET( nk_chart_end, lua_check_nk_context(L,1) )

// NK_API void nk_plot(struct nk_context*, enum nk_chart_type, const float *values, int count, int offset);
LUA_NK_API_NORET( nk_plot, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type)
	, lua_check_nk_float_array_buffer(L,3,lua_check_nk_int(L,4)+lua_check_nk_int(L,5))
	, lua_check_nk_int(L,4), lua_check_nk_int(L,5) )

// NK_API void nk_plot_function(struct nk_context*, enum nk_chart_type, void *userdata, float(*value_getter)(void* user, int index), int count, int offset);
// LUA_NK_API_NORET( nk_plot_function, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_chart_type), lua_topointer(L,3), float(*value_getter)(void* user, int index), lua_check_nk_int(L,5), lua_check_nk_int(L,6) )


// NK_API int nk_popup_begin(struct nk_context*, enum nk_popup_type, const char*, nk_flags, struct nk_rect bounds);
LUA_NK_API( lua_pushboolean, nk_popup_begin, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_popup_type), luaL_checkstring(L,3), lua_check_nk_flags(L,4), *lua_check_nk_rect(L,5) )

// NK_API void nk_popup_close(struct nk_context*);
LUA_NK_API_NORET( nk_popup_close, lua_check_nk_context(L,1) )

// NK_API void nk_popup_end(struct nk_context*);
LUA_NK_API_NORET( nk_popup_end, lua_check_nk_context(L,1) )


// NK_API int nk_combo(struct nk_context*, const char **items, int count, int selected, int item_height, struct nk_vec2 size);
LUA_NK_API_DEFINE( nk_combo, {
	struct nk_string_array* arr = lua_check_nk_string_array(L,2);
	lua_pushinteger(L, nk_combo(lua_check_nk_context(L,1), arr->arr, arr->len, lua_check_nk_int(L,3)
		, lua_check_nk_int(L,4), *lua_check_nk_vec2(L,5)));
	return 1;
} )

// NK_API int nk_combo_separator(struct nk_context*, const char *items_separated_by_separator, int separator, int selected, int count, int item_height, struct nk_vec2 size);
LUA_NK_API( lua_pushinteger, nk_combo_separator, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_char(L,3)
	, lua_check_nk_int(L,4), lua_check_nk_int(L,5), lua_check_nk_int(L,6), *lua_check_nk_vec2(L,7) )

// NK_API int nk_combo_string(struct nk_context*, const char *items_separated_by_zeros, int selected, int count, int item_height, struct nk_vec2 size);
LUA_NK_API( lua_pushinteger, nk_combo_string, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_int(L,3)
	, lua_check_nk_int(L,4), lua_check_nk_int(L,5), *lua_check_nk_vec2(L,6) )

// NK_API int nk_combo_callback(struct nk_context*, void(*item_getter)(void*, int, const char**), void *userdata, int selected, int count, int item_height, struct nk_vec2 size);
// LUA_NK_API( lua_pushinteger, nk_combo_callback, lua_check_nk_context(L,1), void(*item_getter)(void*, int, const char**), void *userdata, int selected, int count, int item_height, *lua_check_nk_vec2(L,3) )

// NK_API void nk_combobox(struct nk_context*, const char **items, int count, int *selected, int item_height, struct nk_vec2 size);
LUA_NK_API_DEFINE( nk_combobox, {
	struct nk_string_array* arr = lua_check_nk_string_array(L,2);
	int selected = lua_check_nk_int(L,3);
	nk_combobox(lua_check_nk_context(L,1), arr->arr, arr->len, &selected, lua_check_nk_int(L,4), *lua_check_nk_vec2(L,5));
	lua_pushinteger(L, selected);
	return 1;
} )

// NK_API void nk_combobox_string(struct nk_context*, const char *items_separated_by_zeros, int *selected, int count, int item_height, struct nk_vec2 size);
LUA_NK_API_DEFINE( nk_combobox_string, {
	int selected = lua_check_nk_int(L,3);
	nk_combobox_string(lua_check_nk_context(L,1), luaL_checkstring(L,2), &selected, lua_check_nk_int(L,4), lua_check_nk_int(L,5), *lua_check_nk_vec2(L,6));
	lua_pushinteger(L, selected);
	return 1;
} )

// NK_API void nk_combobox_separator(struct nk_context*, const char *items_separated_by_separator, int separator,int *selected, int count, int item_height, struct nk_vec2 size);
LUA_NK_API_DEFINE( nk_combobox_separator, {
	int selected = lua_check_nk_int(L,3);
	nk_combobox_separator(lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_char(L,3), &selected
		, lua_check_nk_int(L,5), lua_check_nk_int(L,6), *lua_check_nk_vec2(L,7));
	lua_pushinteger(L, selected);
	return 1;
} )

// NK_API void nk_combobox_callback(struct nk_context*, void(*item_getter)(void*, int, const char**), void*, int *selected, int count, int item_height, struct nk_vec2 size);
// LUA_NK_API_NORET( nk_combobox_callback, lua_check_nk_context(L,1), void(*item_getter)(void*, int, const char**), void*, int *selected, int count, int item_height, *lua_check_nk_vec2(L,3) )


// NK_API int nk_combo_begin_text(struct nk_context*, const char *selected, int, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), *lua_check_nk_vec2(L,4) )

// NK_API int nk_combo_begin_label(struct nk_context*, const char *selected, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_vec2(L,3) )

// NK_API int nk_combo_begin_color(struct nk_context*, struct nk_color color, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_color, lua_check_nk_context(L,1), *lua_check_nk_color(L,2), *lua_check_nk_vec2(L,3) )

// NK_API int nk_combo_begin_symbol(struct nk_context*,  enum nk_symbol_type,  struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_symbol, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type),  *lua_check_nk_vec2(L,3) )

// NK_API int nk_combo_begin_symbol_label(struct nk_context*, const char *selected, enum nk_symbol_type, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_symbol_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_enum(L,3,nk_symbol_type), *lua_check_nk_vec2(L,4) )

// NK_API int nk_combo_begin_symbol_text(struct nk_context*, const char *selected, int, enum nk_symbol_type, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_symbol_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3)
	, lua_check_nk_enum(L,4,nk_symbol_type), *lua_check_nk_vec2(L,5) )

// NK_API int nk_combo_begin_image(struct nk_context*, struct nk_image img,  struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_image, lua_check_nk_context(L,1), *lua_check_nk_image(L,2),  *lua_check_nk_vec2(L,3) )

// NK_API int nk_combo_begin_image_label(struct nk_context*, const char *selected, struct nk_image, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_image_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_image(L,3), *lua_check_nk_vec2(L,4) )

// NK_API int nk_combo_begin_image_text(struct nk_context*,  const char *selected, int, struct nk_image, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_combo_begin_image_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3)
	, *lua_check_nk_image(L,4), *lua_check_nk_vec2(L,5) )

// NK_API int nk_combo_item_label(struct nk_context*, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3) )

// NK_API int nk_combo_item_text(struct nk_context*, const char*,int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_combo_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_image_label, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_combo_item_image_text(struct nk_context*, struct nk_image, const char*, int,nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_image_text, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_combo_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_symbol_label, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_combo_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_combo_item_symbol_text, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API void nk_combo_close(struct nk_context*);
LUA_NK_API_NORET( nk_combo_close, lua_check_nk_context(L,1) )

// NK_API void nk_combo_end(struct nk_context*);
LUA_NK_API_NORET( nk_combo_end, lua_check_nk_context(L,1) )


// NK_API int nk_contextual_begin(struct nk_context*, nk_flags, struct nk_vec2, struct nk_rect trigger_bounds);
LUA_NK_API( lua_pushboolean, nk_contextual_begin, lua_check_nk_context(L,1), lua_check_nk_flags(L,2), *lua_check_nk_vec2(L,3), *lua_check_nk_rect(L,4) )

// NK_API int nk_contextual_item_text(struct nk_context*, const char*, int,nk_flags align);
LUA_NK_API( lua_pushboolean, nk_contextual_item_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_contextual_item_label(struct nk_context*, const char*, nk_flags align);
LUA_NK_API( lua_pushboolean, nk_contextual_item_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3) )

// NK_API int nk_contextual_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_contextual_item_image_label, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_contextual_item_image_text(struct nk_context*, struct nk_image, const char*, int len, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_contextual_item_image_text, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_contextual_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_contextual_item_symbol_label, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_contextual_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_contextual_item_symbol_text, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API void nk_contextual_close(struct nk_context*);
LUA_NK_API_NORET( nk_contextual_close, lua_check_nk_context(L,1) )

// NK_API void nk_contextual_end(struct nk_context*);
LUA_NK_API_NORET( nk_contextual_end, lua_check_nk_context(L,1) )


// NK_API void nk_tooltip(struct nk_context*, const char*);
LUA_NK_API_NORET( nk_tooltip, lua_check_nk_context(L,1), luaL_checkstring(L,2) )

// NK_API int nk_tooltip_begin(struct nk_context*, float width);
LUA_NK_API( lua_pushboolean, nk_tooltip_begin, lua_check_nk_context(L,1), lua_check_nk_float(L,2) )

// NK_API void nk_tooltip_end(struct nk_context*);
LUA_NK_API_NORET( nk_tooltip_end, lua_check_nk_context(L,1) )


// NK_API void nk_menubar_begin(struct nk_context*);
LUA_NK_API_NORET( nk_menubar_begin, lua_check_nk_context(L,1) )

// NK_API void nk_menubar_end(struct nk_context*);
LUA_NK_API_NORET( nk_menubar_end, lua_check_nk_context(L,1) )

// NK_API int nk_menu_begin_text(struct nk_context*, const char* title, int title_len, nk_flags align, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4), *lua_check_nk_vec2(L,5) )

// NK_API int nk_menu_begin_label(struct nk_context*, const char*, nk_flags align, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), *lua_check_nk_vec2(L,4) )

// NK_API int nk_menu_begin_image(struct nk_context*, const char*, struct nk_image, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_image, lua_check_nk_context(L,1), luaL_checkstring(L,2), *lua_check_nk_image(L,3), *lua_check_nk_vec2(L,4) )

// NK_API int nk_menu_begin_image_text(struct nk_context*, const char*, int,nk_flags align,struct nk_image, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_image_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3)
	, lua_check_nk_flags(L,4), *lua_check_nk_image(L,5), *lua_check_nk_vec2(L,6) )

// NK_API int nk_menu_begin_image_label(struct nk_context*, const char*, nk_flags align,struct nk_image, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_image_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3), *lua_check_nk_image(L,4), *lua_check_nk_vec2(L,5) )

// NK_API int nk_menu_begin_symbol(struct nk_context*, const char*, enum nk_symbol_type, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_symbol, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_enum(L,3,nk_symbol_type), *lua_check_nk_vec2(L,4) )

// NK_API int nk_menu_begin_symbol_text(struct nk_context*, const char*, int,nk_flags align,enum nk_symbol_type, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_symbol_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3)
	, lua_check_nk_flags(L,4), lua_check_nk_enum(L,5,nk_symbol_type), *lua_check_nk_vec2(L,6) )

// NK_API int nk_menu_begin_symbol_label(struct nk_context*, const char*, nk_flags align,enum nk_symbol_type, struct nk_vec2 size);
LUA_NK_API( lua_pushboolean, nk_menu_begin_symbol_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3)
	, lua_check_nk_enum(L,4,nk_symbol_type), *lua_check_nk_vec2(L,5) )

// NK_API int nk_menu_item_text(struct nk_context*, const char*, int,nk_flags align);
LUA_NK_API( lua_pushboolean, nk_menu_item_text, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_menu_item_label(struct nk_context*, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_menu_item_label, lua_check_nk_context(L,1), luaL_checkstring(L,2), lua_check_nk_flags(L,3) )

// NK_API int nk_menu_item_image_label(struct nk_context*, struct nk_image, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_menu_item_image_label, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API int nk_menu_item_image_text(struct nk_context*, struct nk_image, const char*, int len, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_menu_item_image_text, lua_check_nk_context(L,1), *lua_check_nk_image(L,2), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_menu_item_symbol_text(struct nk_context*, enum nk_symbol_type, const char*, int, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_menu_item_symbol_text, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_text_len(L,4), lua_check_nk_flags(L,5) )

// NK_API int nk_menu_item_symbol_label(struct nk_context*, enum nk_symbol_type, const char*, nk_flags alignment);
LUA_NK_API( lua_pushboolean, nk_menu_item_symbol_label, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_symbol_type), luaL_checkstring(L,3), lua_check_nk_flags(L,4) )

// NK_API void nk_menu_close(struct nk_context*);
LUA_NK_API_NORET( nk_menu_close, lua_check_nk_context(L,1) )

// NK_API void nk_menu_end(struct nk_context*);
LUA_NK_API_NORET( nk_menu_end, lua_check_nk_context(L,1) )


// NK_API void nk_style_default(struct nk_context*);
LUA_NK_API_NORET( nk_style_default, lua_check_nk_context(L,1) )

// NK_API void nk_style_from_table(struct nk_context*, const struct nk_color*);
LUA_NK_API_NORET( nk_style_from_table, lua_check_nk_context(L,1), lua_check_nk_color(L,2) )

// NK_API void nk_style_load_cursor(struct nk_context*, enum nk_style_cursor, const struct nk_cursor*);
LUA_NK_API_NORET( nk_style_load_cursor, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_style_cursor), lua_check_nk_cursor(L,3) )

// NK_API void nk_style_load_all_cursors(struct nk_context*, struct nk_cursor*);
LUA_NK_API_NORET( nk_style_load_all_cursors, lua_check_nk_context(L,1), lua_check_nk_cursor(L,2) )

// NK_API const char* nk_style_get_color_by_name(enum nk_style_colors);
LUA_NK_API( lua_pushstring, nk_style_get_color_by_name, lua_check_nk_enum(L,1,nk_style_colors) )

// NK_API void nk_style_set_font(struct nk_context*, const struct nk_user_font*);
LUA_NK_API_NORET( nk_style_set_font, lua_check_nk_context(L,1), lua_check_nk_user_font(L,2) )

// NK_API int nk_style_set_cursor(struct nk_context*, enum nk_style_cursor);
LUA_NK_API( lua_pushboolean, nk_style_set_cursor, lua_check_nk_context(L,1), lua_check_nk_enum(L,2,nk_style_cursor) )

// NK_API void nk_style_show_cursor(struct nk_context*);
LUA_NK_API_NORET( nk_style_show_cursor, lua_check_nk_context(L,1) )

// NK_API void nk_style_hide_cursor(struct nk_context*);
LUA_NK_API_NORET( nk_style_hide_cursor, lua_check_nk_context(L,1) )


// NK_API int nk_style_push_font(struct nk_context*, const struct nk_user_font*);
LUA_NK_API( lua_pushboolean, nk_style_push_font, lua_check_nk_context(L,1), lua_check_nk_user_font(L,2) )

// NK_API int nk_style_push_float(struct nk_context*, float*, float);
LUA_NK_API( lua_pushboolean, nk_style_push_float, lua_check_nk_context(L,1), lua_check_nk_float_boxed(L,2), lua_check_nk_float(L,3) )

// NK_API int nk_style_push_vec2(struct nk_context*, struct nk_vec2*, struct nk_vec2);
LUA_NK_API( lua_pushboolean, nk_style_push_vec2, lua_check_nk_context(L,1), lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3) )

// NK_API int nk_style_push_style_item(struct nk_context*, struct nk_style_item*, struct nk_style_item);
LUA_NK_API( lua_pushboolean, nk_style_push_style_item, lua_check_nk_context(L,1), lua_check_nk_style_item(L,2), *lua_check_nk_style_item(L,3) )

// NK_API int nk_style_push_flags(struct nk_context*, nk_flags*, nk_flags);
LUA_NK_API( lua_pushboolean, nk_style_push_flags, lua_check_nk_context(L,1), lua_check_nk_flags_boxed(L,2), lua_check_nk_flags(L,3) )

// NK_API int nk_style_push_color(struct nk_context*, struct nk_color*, struct nk_color);
LUA_NK_API( lua_pushboolean, nk_style_push_color, lua_check_nk_context(L,1), lua_check_nk_color(L,2), *lua_check_nk_color(L,3) )


// NK_API int nk_style_pop_font(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_font, lua_check_nk_context(L,1) )

// NK_API int nk_style_pop_float(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_float, lua_check_nk_context(L,1) )

// NK_API int nk_style_pop_vec2(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_vec2, lua_check_nk_context(L,1) )

// NK_API int nk_style_pop_style_item(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_style_item, lua_check_nk_context(L,1) )

// NK_API int nk_style_pop_flags(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_flags, lua_check_nk_context(L,1) )

// NK_API int nk_style_pop_color(struct nk_context*);
LUA_NK_API( lua_pushboolean, nk_style_pop_color, lua_check_nk_context(L,1) )


// NK_API struct nk_color nk_rgb(int r, int g, int b);
LUA_NK_API( lua_push_nk_color, nk_rgb, lua_check_nk_int(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3) )

// NK_API struct nk_color nk_rgb_iv(const int *rgb);
// LUA_NK_API( lua_push_nk_color, nk_rgb_iv, const int *rgb) )

// NK_API struct nk_color nk_rgb_bv(const nk_byte* rgb);
// LUA_NK_API( lua_push_nk_color, nk_rgb_bv, const nk_byte* rgb) )

// NK_API struct nk_color nk_rgb_f(float r, float g, float b);
LUA_NK_API( lua_push_nk_color, nk_rgb_f, lua_check_nk_float(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3) )

// NK_API struct nk_color nk_rgb_fv(const float *rgb);
// NK_API struct nk_color nk_rgb_hex(const char *rgb);

// NK_API struct nk_color nk_rgba(int r, int g, int b, int a);
LUA_NK_API( lua_push_nk_color, nk_rgba, lua_check_nk_int(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3), lua_check_nk_int(L,4) )

// NK_API struct nk_color nk_rgba_u32(nk_uint);
LUA_NK_API( lua_push_nk_color, nk_rgba_u32, lua_check_nk_uint(L,1) )

// NK_API struct nk_color nk_rgba_iv(const int *rgba);
// NK_API struct nk_color nk_rgba_bv(const nk_byte *rgba);

// NK_API struct nk_color nk_rgba_f(float r, float g, float b, float a);
LUA_NK_API( lua_push_nk_color, nk_rgba_f, lua_check_nk_float(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3), lua_check_nk_float(L,4) )

// NK_API struct nk_color nk_rgba_fv(const float *rgba);
// NK_API struct nk_color nk_rgba_hex(const char *rgb);
LUA_NK_API( lua_push_nk_color, nk_rgba_hex, luaL_checkstring(L,1) )


// NK_API struct nk_color nk_hsv(int h, int s, int v);
LUA_NK_API( lua_push_nk_color, nk_hsv, lua_check_nk_int(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3) )

// NK_API struct nk_color nk_hsv_iv(const int *hsv);
// LUA_NK_API( lua_push_nk_color, nk_hsv_iv, luaL_checkstring(L,1) )

// NK_API struct nk_color nk_hsv_bv(const nk_byte *hsv);
// LUA_NK_API( lua_push_nk_color, nk_hsv_bv, luaL_checkstring(L,1) )

// NK_API struct nk_color nk_hsv_f(float h, float s, float v);
LUA_NK_API( lua_push_nk_color, nk_hsv_f, lua_check_nk_float(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3) )

// NK_API struct nk_color nk_hsv_fv(const float *hsv);
// LUA_NK_API( lua_push_nk_color, nk_hsv_fv, luaL_checkstring(L,1) )


// NK_API struct nk_color nk_hsva(int h, int s, int v, int a);
LUA_NK_API( lua_push_nk_color, nk_hsva, lua_check_nk_int(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3), lua_check_nk_int(L,4) )

// NK_API struct nk_color nk_hsva_iv(const int *hsva);
// NK_API struct nk_color nk_hsva_bv(const nk_byte *hsva);

// NK_API struct nk_color nk_hsva_f(float h, float s, float v, float a);
LUA_NK_API( lua_push_nk_color, nk_hsva_f, lua_check_nk_float(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3), lua_check_nk_float(L,4) )

// NK_API struct nk_color nk_hsva_fv(const float *hsva);


// NK_API void nk_color_f(float *r, float *g, float *b, float *a, struct nk_color);
LUA_NK_API_DEFINE( nk_color_f, {
	float r,g,b,a;
	nk_color_f(&r, &g, &b, &a, *lua_check_nk_color(L,1));
	lua_pushnumber(L,r); lua_pushnumber(L,g); lua_pushnumber(L,b); lua_pushnumber(L,a);
	return 4;
} )

// NK_API void nk_color_fv(float *rgba_out, struct nk_color);

// NK_API void nk_color_d(double *r, double *g, double *b, double *a, struct nk_color);
LUA_NK_API_DEFINE( nk_color_d, {
	double r,g,b,a;
	nk_color_d(&r, &g, &b, &a, *lua_check_nk_color(L,1));
	lua_pushnumber(L,r); lua_pushnumber(L,g); lua_pushnumber(L,b); lua_pushnumber(L,a);
	return 4;
} )

// NK_API void nk_color_dv(double *rgba_out, struct nk_color);

// NK_API nk_uint nk_color_u32(struct nk_color);
LUA_NK_API( lua_pushinteger, nk_color_u32, *lua_check_nk_color(L,1) )

// NK_API void nk_color_hex_rgba(char *output, struct nk_color);
LUA_NK_API_DEFINE( nk_color_hex_rgba, {
	char s[16];
	nk_color_hex_rgba(s, *lua_check_nk_color(L,1));
	lua_pushstring(L,s);
	return 1;
} )

// NK_API void nk_color_hex_rgb(char *output, struct nk_color);
LUA_NK_API_DEFINE( nk_color_hex_rgb, {
	char s[16];
	nk_color_hex_rgb(s, *lua_check_nk_color(L,1));
	lua_pushstring(L,s);
	return 1;
} )

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
LUA_NK_API_NORET( nk_triangle_from_direction, lua_check_nk_vec2(L,1), *lua_check_nk_rect(L,2), lua_check_nk_float(L,3), lua_check_nk_float(L,4), lua_check_nk_enum(L,5,nk_heading) )


// NK_API struct nk_vec2 nk_vec2(float x, float y);
LUA_NK_API( lua_push_nk_vec2, nk_vec2, lua_check_nk_float(L,1), lua_check_nk_float(L,2) )

// NK_API struct nk_vec2 nk_vec2i(int x, int y);
LUA_NK_API( lua_push_nk_vec2, nk_vec2i, lua_check_nk_int(L,1), lua_check_nk_int(L,2) )

// NK_API struct nk_vec2 nk_vec2v(const float *xy);
// NK_API struct nk_vec2 nk_vec2iv(const int *xy);

// NK_API struct nk_rect nk_get_null_rect(void);
LUA_NK_API( lua_push_nk_rect, nk_get_null_rect )

// NK_API struct nk_rect nk_rect(float x, float y, float w, float h);
LUA_NK_API( lua_push_nk_rect, nk_rect, lua_check_nk_float(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3), lua_check_nk_float(L,4) )

// NK_API struct nk_rect nk_recti(int x, int y, int w, int h);
LUA_NK_API( lua_push_nk_rect, nk_recti, lua_check_nk_int(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3), lua_check_nk_int(L,4) )

// NK_API struct nk_rect nk_recta(struct nk_vec2 pos, struct nk_vec2 size);
LUA_NK_API( lua_push_nk_rect, nk_recta, *lua_check_nk_vec2(L,1), *lua_check_nk_vec2(L,2) )

// NK_API struct nk_rect nk_rectv(const float *xywh);
// NK_API struct nk_rect nk_rectiv(const int *xywh);

// NK_API struct nk_vec2 nk_rect_pos(struct nk_rect);
LUA_NK_API( lua_push_nk_vec2, nk_rect_pos, *lua_check_nk_rect(L,1) )

// NK_API struct nk_vec2 nk_rect_size(struct nk_rect);
LUA_NK_API( lua_push_nk_vec2, nk_rect_size, *lua_check_nk_rect(L,1) )


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
// LUA_NK_API_NORET( nk_font_atlas_init_default, lua_check_nk_font_atlas(L,1) )
#endif
// NK_API void nk_font_atlas_init(struct nk_font_atlas*, struct nk_allocator*);
// NK_API void nk_font_atlas_init_custom(struct nk_font_atlas*, struct nk_allocator *persistent, struct nk_allocator *transient);

// NK_API void nk_font_atlas_begin(struct nk_font_atlas*);
LUA_NK_API_NORET( nk_font_atlas_begin, lua_check_nk_font_atlas(L,1) )

// NK_API struct nk_font_config nk_font_config(float pixel_height);

// NK_API struct nk_font *nk_font_atlas_add(struct nk_font_atlas*, const struct nk_font_config*);
LUA_NK_API(lua_push_nk_font_ptr, nk_font_atlas_add, lua_check_nk_font_atlas(L,1), lua_check_nk_font_config(L,2) )

#ifdef NK_INCLUDE_DEFAULT_FONT
// NK_API struct nk_font* nk_font_atlas_add_default(struct nk_font_atlas*, float height, const struct nk_font_config*);
LUA_NK_API(lua_push_nk_font_ptr, nk_font_atlas_add_default, lua_check_nk_font_atlas(L,1), lua_check_nk_float(L,2), lua_check_nk_font_config(L,3) )

#endif
// NK_API struct nk_font* nk_font_atlas_add_from_memory(struct nk_font_atlas *atlas, void *memory, nk_size size, float height, const struct nk_font_config *config);
#ifdef NK_INCLUDE_STANDARD_IO
// NK_API struct nk_font* nk_font_atlas_add_from_file(struct nk_font_atlas *atlas, const char *file_path, float height, const struct nk_font_config*);
LUA_NK_API(lua_push_nk_font_ptr, nk_font_atlas_add_from_file, lua_check_nk_font_atlas(L,1), luaL_checkstring(L,2), lua_check_nk_float(L,3), lua_check_nk_font_config(L,4) )

#endif
// NK_API struct nk_font *nk_font_atlas_add_compressed(struct nk_font_atlas*, void *memory, nk_size size, float height, const struct nk_font_config*);
// NK_API struct nk_font* nk_font_atlas_add_compressed_base85(struct nk_font_atlas*, const char *data, float height, const struct nk_font_config *config);
// NK_API const void* nk_font_atlas_bake(struct nk_font_atlas*, int *width, int *height, enum nk_font_atlas_format);
// NK_API void nk_font_atlas_end(struct nk_font_atlas*, nk_handle tex, struct nk_draw_null_texture*);
// NK_API const struct nk_font_glyph* nk_font_find_glyph(struct nk_font*, nk_rune unicode);
// NK_API void nk_font_atlas_cleanup(struct nk_font_atlas *atlas);
LUA_NK_API_NORET( nk_font_atlas_cleanup, lua_check_nk_font_atlas(L,1) )

// NK_API void nk_font_atlas_clear(struct nk_font_atlas*);
LUA_NK_API_NORET( nk_font_atlas_clear, lua_check_nk_font_atlas(L,1) )


#endif

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
// NK_API void nk_buffer_init_default(struct nk_buffer*);
// LUA_NK_API_NORET( nk_buffer_init_default, lua_check_nk_buffer(L,1) )
#endif
// NK_API void nk_buffer_init(struct nk_buffer*, const struct nk_allocator*, nk_size size);
// NK_API void nk_buffer_init_fixed(struct nk_buffer*, void *memory, nk_size size);
// NK_API void nk_buffer_info(struct nk_memory_status*, struct nk_buffer*);
// NK_API void nk_buffer_push(struct nk_buffer*, enum nk_buffer_allocation_type type, const void *memory, nk_size size, nk_size align);
// NK_API void nk_buffer_mark(struct nk_buffer*, enum nk_buffer_allocation_type type);
LUA_NK_API_NORET( nk_buffer_mark, lua_check_nk_buffer(L,1), lua_check_nk_enum(L,2,nk_buffer_allocation_type) )

// NK_API void nk_buffer_reset(struct nk_buffer*, enum nk_buffer_allocation_type type);
LUA_NK_API_NORET( nk_buffer_reset, lua_check_nk_buffer(L,1), lua_check_nk_enum(L,2,nk_buffer_allocation_type) )

// NK_API void nk_buffer_clear(struct nk_buffer*);
LUA_NK_API_NORET( nk_buffer_clear, lua_check_nk_buffer(L,1) )

// NK_API void nk_buffer_free(struct nk_buffer*);
// LUA_NK_API_NORET( nk_buffer_free, lua_check_nk_buffer(L,1) )

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
// LUA_NK_API_NORET( nk_textedit_init_default, lua_check_nk_text_edit(L,1) )
#endif
// NK_API void nk_textedit_init(struct nk_text_edit*, struct nk_allocator*, nk_size size);
// NK_API void nk_textedit_init_fixed(struct nk_text_edit*, void *memory, nk_size size);

// NK_API void nk_textedit_free(struct nk_text_edit*);
// LUA_NK_API_NORET( nk_textedit_free, lua_check_nk_text_edit(L,1) )

// NK_API void nk_textedit_text(struct nk_text_edit*, const char*, int total_len);
LUA_NK_API_NORET( nk_textedit_text, lua_check_nk_text_edit(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3) )

// NK_API void nk_textedit_delete(struct nk_text_edit*, int where, int len);
LUA_NK_API_NORET( nk_textedit_delete, lua_check_nk_text_edit(L,1), lua_check_nk_int(L,2), lua_check_nk_int(L,3) )

// NK_API void nk_textedit_delete_selection(struct nk_text_edit*);
LUA_NK_API_NORET( nk_textedit_delete_selection, lua_check_nk_text_edit(L,1) )

// NK_API void nk_textedit_select_all(struct nk_text_edit*);
LUA_NK_API_NORET( nk_textedit_select_all, lua_check_nk_text_edit(L,1) )

// NK_API int nk_textedit_cut(struct nk_text_edit*);
LUA_NK_API( lua_pushboolean, nk_textedit_cut, lua_check_nk_text_edit(L,1) )

// NK_API int nk_textedit_paste(struct nk_text_edit*, char const*, int len);
LUA_NK_API( lua_pushboolean, nk_textedit_paste, lua_check_nk_text_edit(L,1), luaL_checkstring(L,2), lua_check_nk_text_len(L,3) )

// NK_API void nk_textedit_undo(struct nk_text_edit*);
LUA_NK_API_NORET( nk_textedit_undo, lua_check_nk_text_edit(L,1) )

// NK_API void nk_textedit_redo(struct nk_text_edit*);
LUA_NK_API_NORET( nk_textedit_redo, lua_check_nk_text_edit(L,1) )


// NK_API void nk_stroke_line(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_line, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, *lua_check_nk_color(L,7) )

// NK_API void nk_stroke_curve(struct nk_command_buffer*, float, float, float, float, float, float, float, float, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_curve, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, lua_check_nk_float(L,7), lua_check_nk_float(L,8), lua_check_nk_float(L,9)
	, lua_check_nk_float(L,10), *lua_check_nk_color(L,11) )

// NK_API void nk_stroke_rect(struct nk_command_buffer*, struct nk_rect, float rounding, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_rect, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), lua_check_nk_float(L,3), lua_check_nk_float(L,4), *lua_check_nk_color(L,5) )

// NK_API void nk_stroke_circle(struct nk_command_buffer*, struct nk_rect, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_circle, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), lua_check_nk_float(L,3), *lua_check_nk_color(L,4) )

// NK_API void nk_stroke_arc(struct nk_command_buffer*, float cx, float cy, float radius, float a_min, float a_max, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_arc, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, lua_check_nk_float(L,7), *lua_check_nk_color(L,8) )

// NK_API void nk_stroke_triangle(struct nk_command_buffer*, float, float, float, float, float, float, float line_thichness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_triangle, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, lua_check_nk_float(L,7), lua_check_nk_float(L,8), *lua_check_nk_color(L,9) )

// NK_API void nk_stroke_polyline(struct nk_command_buffer*, float *points, int point_count, float line_thickness, struct nk_color col);
LUA_NK_API_NORET( nk_stroke_polyline, lua_check_nk_command_buffer(L,1), lua_check_nk_float_array_buffer(L,2,2*lua_check_nk_int(L,3)), lua_check_nk_int(L,3)
	, lua_check_nk_float(L,4), *lua_check_nk_color(L,5) )

// NK_API void nk_stroke_polygon(struct nk_command_buffer*, float*, int point_count, float line_thickness, struct nk_color);
LUA_NK_API_NORET( nk_stroke_polygon, lua_check_nk_command_buffer(L,1), lua_check_nk_float_array_buffer(L,2,2*lua_check_nk_int(L,3)), lua_check_nk_int(L,3)
	, lua_check_nk_float(L,4), *lua_check_nk_color(L,5) )



// NK_API void nk_fill_rect(struct nk_command_buffer*, struct nk_rect, float rounding, struct nk_color);
LUA_NK_API_NORET( nk_fill_rect, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), lua_check_nk_float(L,3), *lua_check_nk_color(L,4) )

// NK_API void nk_fill_rect_multi_color(struct nk_command_buffer*, struct nk_rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom);
LUA_NK_API_NORET( nk_fill_rect_multi_color, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3)
	, *lua_check_nk_color(L,4), *lua_check_nk_color(L,5), *lua_check_nk_color(L,6) )

// NK_API void nk_fill_circle(struct nk_command_buffer*, struct nk_rect, struct nk_color);
LUA_NK_API_NORET( nk_fill_circle, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3) )

// NK_API void nk_fill_arc(struct nk_command_buffer*, float cx, float cy, float radius, float a_min, float a_max, struct nk_color);
LUA_NK_API_NORET( nk_fill_arc, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, *lua_check_nk_color(L,7) )

// NK_API void nk_fill_triangle(struct nk_command_buffer*, float x0, float y0, float x1, float y1, float x2, float y2, struct nk_color);
LUA_NK_API_NORET( nk_fill_triangle, lua_check_nk_command_buffer(L,1), lua_check_nk_float(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_float(L,6)
	, lua_check_nk_float(L,7), *lua_check_nk_color(L,8) )

// NK_API void nk_fill_polygon(struct nk_command_buffer*, float*, int point_count, struct nk_color);
LUA_NK_API_NORET( nk_fill_polygon, lua_check_nk_command_buffer(L,1), lua_check_nk_float_array_buffer(L,2,2*lua_check_nk_int(L,3)), lua_check_nk_int(L,3), *lua_check_nk_color(L,4) )


// NK_API void nk_draw_image(struct nk_command_buffer*, struct nk_rect, const struct nk_image*, struct nk_color);
LUA_NK_API_NORET( nk_draw_image, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), lua_check_nk_image(L,3), *lua_check_nk_color(L,4) )

// NK_API void nk_draw_text(struct nk_command_buffer*, struct nk_rect, const char *text, int len, const struct nk_user_font*, struct nk_color, struct nk_color);
LUA_NK_API_NORET( nk_draw_text, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2), luaL_checkstring(L,3), lua_check_nk_text_len(L,4)
	, lua_check_nk_user_font(L,5), *lua_check_nk_color(L,6), *lua_check_nk_color(L,7) )

// NK_API void nk_push_scissor(struct nk_command_buffer*, struct nk_rect);
LUA_NK_API_NORET( nk_push_scissor, lua_check_nk_command_buffer(L,1), *lua_check_nk_rect(L,2) )

// NK_API void nk_push_custom(struct nk_command_buffer*, struct nk_rect, nk_command_custom_callback, nk_handle usr);

// NK_API int nk_input_has_mouse_click(const struct nk_input*, enum nk_buttons);
LUA_NK_API( lua_pushboolean, nk_input_has_mouse_click, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons) )

// NK_API int nk_input_has_mouse_click_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_has_mouse_click_in_rect, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons), *lua_check_nk_rect(L,3) )

// NK_API int nk_input_has_mouse_click_down_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect, int down);
LUA_NK_API( lua_pushboolean, nk_input_has_mouse_click_down_in_rect, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons), *lua_check_nk_rect(L,3), lua_toboolean(L,4) )

// NK_API int nk_input_is_mouse_click_in_rect(const struct nk_input*, enum nk_buttons, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_click_in_rect, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons), *lua_check_nk_rect(L,3) )

// NK_API int nk_input_is_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b, int down);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_click_down_in_rect, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons), *lua_check_nk_rect(L,3), lua_toboolean(L,4) )

// NK_API int nk_input_any_mouse_click_in_rect(const struct nk_input*, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_any_mouse_click_in_rect, lua_check_nk_input(L,1), *lua_check_nk_rect(L,2) )

// NK_API int nk_input_is_mouse_prev_hovering_rect(const struct nk_input*, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_prev_hovering_rect, lua_check_nk_input(L,1), *lua_check_nk_rect(L,2) )

// NK_API int nk_input_is_mouse_hovering_rect(const struct nk_input*, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_hovering_rect, lua_check_nk_input(L,1), *lua_check_nk_rect(L,2) )

// NK_API int nk_input_mouse_clicked(const struct nk_input*, enum nk_buttons, struct nk_rect);
LUA_NK_API( lua_pushboolean, nk_input_mouse_clicked, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons), *lua_check_nk_rect(L,3) )

// NK_API int nk_input_is_mouse_down(const struct nk_input*, enum nk_buttons);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_down, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons) )

// NK_API int nk_input_is_mouse_pressed(const struct nk_input*, enum nk_buttons);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_pressed, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons) )

// NK_API int nk_input_is_mouse_released(const struct nk_input*, enum nk_buttons);
LUA_NK_API( lua_pushboolean, nk_input_is_mouse_released, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_buttons) )

// NK_API int nk_input_is_key_pressed(const struct nk_input*, enum nk_keys);
LUA_NK_API( lua_pushboolean, nk_input_is_key_pressed, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_keys) )

// NK_API int nk_input_is_key_released(const struct nk_input*, enum nk_keys);
LUA_NK_API( lua_pushboolean, nk_input_is_key_released, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_keys) )

// NK_API int nk_input_is_key_down(const struct nk_input*, enum nk_keys);
LUA_NK_API( lua_pushboolean, nk_input_is_key_down, lua_check_nk_input(L,1), lua_check_nk_enum(L,2,nk_keys) )


#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT

// NK_API void nk_draw_list_init(struct nk_draw_list*);
LUA_NK_API_NORET( nk_draw_list_init, lua_check_nk_draw_list(L,1) )

// NK_API void nk_draw_list_setup(struct nk_draw_list*, const struct nk_convert_config*, struct nk_buffer *cmds, struct nk_buffer *vertices, struct nk_buffer *elements, enum nk_anti_aliasing line_aa,enum nk_anti_aliasing shape_aa);

// NK_API void nk_draw_list_clear(struct nk_draw_list*);
LUA_NK_API_NORET( nk_draw_list_clear, lua_check_nk_draw_list(L,1) )


// #define nk_draw_list_foreach(cmd, can, b) for((cmd)=nk__draw_list_begin(can, b); (cmd)!=0; (cmd)=nk__draw_list_next(cmd, b, can) )
// NK_API const struct nk_draw_command* nk__draw_list_begin(const struct nk_draw_list*, const struct nk_buffer*);
// NK_API const struct nk_draw_command* nk__draw_list_next(const struct nk_draw_command*, const struct nk_buffer*, const struct nk_draw_list*);
// NK_API const struct nk_draw_command* nk__draw_list_end(const struct nk_draw_list*, const struct nk_buffer*);


// NK_API void nk_draw_list_path_clear(struct nk_draw_list*);
LUA_NK_API_NORET( nk_draw_list_path_clear, lua_check_nk_draw_list(L,1) )

// NK_API void nk_draw_list_path_line_to(struct nk_draw_list*, struct nk_vec2 pos);
LUA_NK_API_NORET( nk_draw_list_path_line_to, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2) )

// NK_API void nk_draw_list_path_arc_to_fast(struct nk_draw_list*, struct nk_vec2 center, float radius, int a_min, int a_max);
LUA_NK_API_NORET( nk_draw_list_path_arc_to_fast, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), lua_check_nk_float(L,3), lua_check_nk_int(L,4), lua_check_nk_int(L,5) )

// NK_API void nk_draw_list_path_arc_to(struct nk_draw_list*, struct nk_vec2 center, float radius, float a_min, float a_max, unsigned int segments);
LUA_NK_API_NORET( nk_draw_list_path_arc_to, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), lua_check_nk_float(L,3)
	, lua_check_nk_float(L,4), lua_check_nk_float(L,5), lua_check_nk_uint(L,6) )

// NK_API void nk_draw_list_path_rect_to(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, float rounding);
LUA_NK_API_NORET( nk_draw_list_path_rect_to, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3), lua_check_nk_float(L,4) )

// NK_API void nk_draw_list_path_curve_to(struct nk_draw_list*, struct nk_vec2 p2, struct nk_vec2 p3, struct nk_vec2 p4, unsigned int num_segments);
LUA_NK_API_NORET( nk_draw_list_path_curve_to, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3), *lua_check_nk_vec2(L,4), lua_check_nk_uint(L,5) )

// NK_API void nk_draw_list_path_fill(struct nk_draw_list*, struct nk_color);
LUA_NK_API_NORET( nk_draw_list_path_fill, lua_check_nk_draw_list(L,1), *lua_check_nk_color(L,2) )

// NK_API void nk_draw_list_path_stroke(struct nk_draw_list*, struct nk_color, enum nk_draw_list_stroke closed, float thickness);
LUA_NK_API_NORET( nk_draw_list_path_stroke, lua_check_nk_draw_list(L,1), *lua_check_nk_color(L,2), lua_check_nk_enum(L,3,nk_draw_list_stroke), lua_check_nk_float(L,4) )


// NK_API void nk_draw_list_stroke_line(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_color, float thickness);
LUA_NK_API_NORET( nk_draw_list_stroke_line, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,2), *lua_check_nk_color(L,3), lua_check_nk_float(L,4) )

// NK_API void nk_draw_list_stroke_rect(struct nk_draw_list*, struct nk_rect rect, struct nk_color, float rounding, float thickness);
LUA_NK_API_NORET( nk_draw_list_stroke_rect, lua_check_nk_draw_list(L,1), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3), lua_check_nk_float(L,4), lua_check_nk_float(L,5) )

// NK_API void nk_draw_list_stroke_triangle(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, struct nk_color, float thickness);
LUA_NK_API_NORET( nk_draw_list_stroke_triangle, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3)
	, *lua_check_nk_vec2(L,4), *lua_check_nk_color(L,5), lua_check_nk_float(L,6) )

// NK_API void nk_draw_list_stroke_circle(struct nk_draw_list*, struct nk_vec2 center, float radius, struct nk_color, unsigned int segs, float thickness);
LUA_NK_API_NORET( nk_draw_list_stroke_circle, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), lua_check_nk_float(L,3)
	, *lua_check_nk_color(L,4), lua_check_nk_uint(L,5), lua_check_nk_float(L,6) )

// NK_API void nk_draw_list_stroke_curve(struct nk_draw_list*, struct nk_vec2 p0, struct nk_vec2 cp0, struct nk_vec2 cp1, struct nk_vec2 p1, struct nk_color, unsigned int segments, float thickness);
LUA_NK_API_NORET( nk_draw_list_stroke_curve, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3)
	, *lua_check_nk_vec2(L,4), *lua_check_nk_vec2(L,5), *lua_check_nk_color(L,6)
	, lua_check_nk_uint(L,7), lua_check_nk_float(L,8) )

// NK_API void nk_draw_list_stroke_poly_line(struct nk_draw_list*, const struct nk_vec2 *pnts, const unsigned int cnt, struct nk_color, enum nk_draw_list_stroke, float thickness, enum nk_anti_aliasing);
LUA_NK_API_NORET( nk_draw_list_stroke_poly_line, lua_check_nk_draw_list(L,1)
	, lua_check_nk_vec2_array_buffer(L,2,lua_check_nk_uint(L,3)), lua_check_nk_uint(L,3)
	, *lua_check_nk_color(L,4), lua_check_nk_enum(L,5,nk_draw_list_stroke)
	, lua_check_nk_float(L,6), lua_check_nk_enum(L,7,nk_anti_aliasing) )


// NK_API void nk_draw_list_fill_rect(struct nk_draw_list*, struct nk_rect rect, struct nk_color, float rounding);
LUA_NK_API_NORET( nk_draw_list_fill_rect, lua_check_nk_draw_list(L,1), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3), lua_check_nk_float(L,4) )

// NK_API void nk_draw_list_fill_rect_multi_color(struct nk_draw_list*, struct nk_rect rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom);
LUA_NK_API_NORET( nk_draw_list_fill_rect_multi_color, lua_check_nk_draw_list(L,1), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3)
	, *lua_check_nk_color(L,4), *lua_check_nk_color(L,5), *lua_check_nk_color(L,6) )

// NK_API void nk_draw_list_fill_triangle(struct nk_draw_list*, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, *lua_check_nk_color(L,3));
LUA_NK_API_NORET( nk_draw_list_fill_triangle, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), *lua_check_nk_vec2(L,3), *lua_check_nk_vec2(L,4), *lua_check_nk_color(L,5) )

// NK_API void nk_draw_list_fill_circle(struct nk_draw_list*, struct nk_vec2 center, float radius, struct nk_color col, unsigned int segs);
LUA_NK_API_NORET( nk_draw_list_fill_circle, lua_check_nk_draw_list(L,1), *lua_check_nk_vec2(L,2), lua_check_nk_float(L,3), *lua_check_nk_color(L,4), lua_check_nk_uint(L,5) )

// NK_API void nk_draw_list_fill_poly_convex(struct nk_draw_list*, const struct nk_vec2 *points, const unsigned int count, struct nk_color, enum nk_anti_aliasing);
LUA_NK_API_NORET( nk_draw_list_fill_poly_convex, lua_check_nk_draw_list(L,1)
	, lua_check_nk_vec2_array_buffer(L,2,lua_check_nk_uint(L,3)), lua_check_nk_uint(L,3)
	, *lua_check_nk_color(L,4), lua_check_nk_enum(L,5,nk_anti_aliasing) )


// NK_API void nk_draw_list_add_image(struct nk_draw_list*, struct nk_image texture, struct nk_rect rect, struct nk_color);
LUA_NK_API_NORET( nk_draw_list_add_image, lua_check_nk_draw_list(L,1), *lua_check_nk_image(L,4), *lua_check_nk_rect(L,2), *lua_check_nk_color(L,3) )

// NK_API void nk_draw_list_add_text(struct nk_draw_list*, const struct nk_user_font*, struct nk_rect, const char *text, int len, float font_height, struct nk_color);
LUA_NK_API_NORET( nk_draw_list_add_text, lua_check_nk_draw_list(L,1), lua_check_nk_user_font(L,2), *lua_check_nk_rect(L,3)
	, luaL_checkstring(L,4), lua_check_nk_text_len(L,5), lua_check_nk_float(L,6), *lua_check_nk_color(L,7) )

#ifdef NK_INCLUDE_COMMAND_USERDATA
// NK_API void nk_draw_list_push_userdata(struct nk_draw_list*, nk_handle userdata);
#endif

#endif

// NK_API struct nk_style_item nk_style_item_image(struct nk_image img);
LUA_NK_API( lua_push_nk_style_item, nk_style_item_image, *lua_check_nk_image(L,1) )

// NK_API struct nk_style_item nk_style_item_color(struct nk_color);
LUA_NK_API( lua_push_nk_style_item, nk_style_item_color, *lua_check_nk_color(L,1) )

// NK_API struct nk_style_item nk_style_item_hide(void);
LUA_NK_API( lua_push_nk_style_item, nk_style_item_hide )

#undef LUA_NK_API_DEFINE
#undef LUA_NK_API_NORET
#undef LUA_NK_API

