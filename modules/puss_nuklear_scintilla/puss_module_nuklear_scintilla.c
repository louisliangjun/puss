// puss_module_nuklear_scintilla.c

#include "puss_module_nuklear.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "scintilla_nk.h"

#define LUA_NK_SCI_NAME      "ScintillaNK"

typedef enum _IFaceType
	{ IFaceType_void			// void
	, IFaceType_int				// int
	, IFaceType_bool			// bool -> integer, 1=true, 0=false
	, IFaceType_position		// position -> integer position in a document
	, IFaceType_colour			// colour -> colour integer containing red, green and blue bytes.
	, IFaceType_string			// string -> pointer to const character
	, IFaceType_stringresult	// stringresult -> pointer to character, NULL-> return size of result
	, IFaceType_cells			// cells -> pointer to array of cells, each cell containing a style byte and character byte
	, IFaceType_textrange		// textrange -> range of a min and a max position with an output string
	, IFaceType_findtext		// findtext -> searchrange, text -> foundposition
	, IFaceType_keymod			// keymod -> integer containing key in low half and modifiers in high half
	, IFaceType_formatrange		// formatrange
	, IFaceTypeMax
	} IFaceType;

static const char* iface_type_name[] =
	{ "void"
	, "int"
	, "bool"
	, "position"
	, "colour"
	, "string"
	, "stringresult"
	, "cells"
	, "textrange"
	, "findtext"
	, "keymod"
	, "formatrange"
	};

typedef struct _IFaceParam {
	int					vint;
	int					vbool;
	int					vposition;
	unsigned			vcolour;
	const char*			vstring;
} IFaceParam;

typedef struct _IFaceDecl {
	const char*			name;
	const char*			alias;
	unsigned int		message;
	IFaceType			rtype;
	IFaceType			wparam;
	IFaceType			lparam;
} IFaceDecl;

typedef struct _IFaceVal {
	const char*			name;
	int					val;
} IFaceVal;

static int _lua_nk_scintilla_new(lua_State* L) {
	ScintillaNK** ud;
	ScintillaNK* sci = scintilla_nk_new();
	if( !sci ) {
		lua_pushnil(L);
		return 0;
	}

	ud = (ScintillaNK**)lua_newuserdata(L, sizeof(ScintillaNK*));
	*ud = sci;
	luaL_setmetatable(L, LUA_NK_SCI_NAME);
	return 1;
}

static int _lua_nk_scintilla_free(lua_State* L) {
	ScintillaNK** ud = (ScintillaNK**)luaL_checkudata(L, 1, LUA_NK_SCI_NAME);
	ScintillaNK* sci = *ud;
	if( sci ) {
		*ud = NULL;
		scintilla_nk_free(sci);
	}
	return 0;
}

static int _lua_nk_scintilla_update(lua_State* L) {
	struct nk_context* ctx = __puss_nuklear_iface__->check_nk_context(L, 1);
	ScintillaNK** ud = (ScintillaNK**)luaL_checkudata(L, 2, LUA_NK_SCI_NAME);
	ScintillaNK* sci = *ud;
	if( sci && ctx ) {
		scintilla_nk_update(sci, ctx);
	}
	return 0;
}

static int _lua__sci_send_wrap(lua_State* L) {
	IFaceDecl* decl = (IFaceDecl*)lua_touserdata(L, lua_upvalueindex(1));
	ScintillaNK** ud = (ScintillaNK**)luaL_checkudata(L, 1, LUA_NK_SCI_NAME);
	int nret = 0;
	uptr_t wparam = 0;
	sptr_t lparam = 0;
	if( *ud )
		return luaL_argerror(L, 1, "ScintillaNK already free!");

	switch( decl->wparam ) {
	case IFaceType_void:	break;
	case IFaceType_bool:	wparam = (uptr_t)lua_toboolean(L, 2);		break;
	case IFaceType_colour:
	case IFaceType_position:
	case IFaceType_int:		wparam = (uptr_t)luaL_checknumber(L, 2);	break;
	case IFaceType_string:	wparam = (uptr_t)luaL_checkstring(L, 2);	break;
	default:
		if( decl->wparam < IFaceTypeMax )
			return luaL_error(L, "not support wparam type(%s)", iface_type_name[decl->wparam]);
		return luaL_error(L, "not support wparam type(%d) system error!", decl->wparam);
	}

	switch( decl->lparam ) {
	case IFaceType_void:	break;
	case IFaceType_bool:	lparam = (sptr_t)lua_toboolean(L, 3);		break;
	case IFaceType_colour:
	case IFaceType_position:
	case IFaceType_int:		lparam = (sptr_t)luaL_checknumber(L, 3);	break;
	case IFaceType_string:	lparam = (sptr_t)luaL_checkstring(L, 3);	break;
	case IFaceType_stringresult:
		{
			int len = (int)scintilla_nk_send(*ud, decl->message, wparam, lparam);
			if( len <= 0 )
				return luaL_error(L, "lparam stringresult fetch length failed!");
			wparam = (uptr_t)len;
			lparam = (sptr_t)lua_newuserdata(L, (size_t)len);
		}
		break;
	default:
		if( decl->lparam < IFaceTypeMax )
			return luaL_error(L, "not support lparam type(%s)", iface_type_name[decl->lparam]);
		return luaL_error(L, "not support lparam type(%d) system error!", decl->lparam);
	}

	sptr_t ret = scintilla_nk_send(*ud, decl->message, wparam, lparam);
	switch( decl->rtype ) {
	case IFaceType_void:	break;
	case IFaceType_bool:	lua_pushboolean(L, (int)ret);	++nret;	break;
	case IFaceType_colour:
	case IFaceType_position:
	case IFaceType_int:		lua_pushnumber(L, (lua_Number)ret);	++nret;	break;
	default:
		if( decl->rtype < IFaceTypeMax )
			return luaL_error(L, "not support return type(%s)", iface_type_name[decl->rtype]);
		return luaL_error(L, "not support return type(%d) system error!", decl->rtype);
	}

	if( decl->lparam==IFaceType_stringresult ) {
		lua_pushlstring(L, (const char*)lparam, (size_t)ret);
		++nret;
	}

	return nret;
}

#include "scintilla.iface.inl"

PussInterface* __puss_iface__ = NULL;
PussNuklearInterface* __puss_nuklear_iface__ = NULL;

static void custom_draw_test(struct nk_context * ctx) {
	struct nk_command_buffer *out = &(ctx->current->buffer);
	struct nk_rect bounds;
	float rounding = 3.0f;
	struct nk_color background = { 0xff, 0xff, 0x00, 0x7f };
	// enum nk_widget_layout_states state = 
	nk_widget(&bounds, ctx);
	nk_fill_rect(out, bounds, rounding, background);
}

static int nk_scintilla_demo(lua_State* L) {
	struct nk_context * ctx = __puss_nuklear_iface__->check_nk_context(L, 1);
    if (nk_begin(ctx, "Demo", __nk_recti(50, 50, 230, 250),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        enum {EASY, HARD};
        static int op = EASY;
        static int property = 20;
     	static struct nk_color background;
        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button"))
            fprintf(stdout, "button pressed\n");

        nk_layout_row_dynamic(ctx, 30, 2);
        if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
        if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;

        nk_layout_row_dynamic(ctx, 25, 1);
        nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_combo_begin_color(ctx, background, __nk_vec2i(nk_widget_width(ctx),400))) {
            nk_layout_row_dynamic(ctx, 120, 1);
            background = nk_color_picker(ctx, background, NK_RGBA);
            nk_layout_row_dynamic(ctx, 25, 1);
            background.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, background.r, 255, 1,1);
            background.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, background.g, 255, 1,1);
            background.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, background.b, 255, 1,1);
            background.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, background.a, 255, 1,1);
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 20, 1);
        custom_draw_test(ctx);
    }
    nk_end(ctx);
    return 0;
}

static luaL_Reg nk_scintilla_lib_methods[] =
	{ {"nk_scintilla_new", _lua_nk_scintilla_new}
	, {"nk_scintilla_free", _lua_nk_scintilla_free}
	, {"nk_scintilla_update", _lua_nk_scintilla_update}
	, {"nk_scintilla_demo", nk_scintilla_demo}
	, {NULL, NULL}
	};

/* debug:
set breakpoint pending on
b __puss_module_init__
*/ 
PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;

	puss_module_require(L, "puss_nuklear");	// push nk

	if( !__puss_nuklear_iface__ ) {
		__puss_nuklear_iface__ = puss_interface_check(L, PussNuklearInterface);
	}

	if( luaL_getmetatable(L, LUA_NK_SCI_NAME)==LUA_TTABLE ) {
		lua_pop(L, 1);
		return 1;
	}
	lua_pop(L, 1);

	// const: vals
	puss_push_const_table(L);
	{
		IFaceVal* p;
		for( p=sci_values; p->name; ++p ) {
			lua_pushinteger(L, p->val);
			lua_setfield(L, -2, p->name);
		}
	}
	lua_pop(L, 1);

	// metatable: fun/get/set
	if( luaL_newmetatable(L, LUA_NK_SCI_NAME) ) {
		IFaceDecl* p;
		for( p=sci_functions; p->name; ++p ) {
			lua_pushlightuserdata(L, p);
			lua_pushcclosure(L, _lua__sci_send_wrap, 1);
			lua_setfield(L, -2, p->alias);	// use alias only
		}
	}
	lua_setfield(L, -1, "__index");

	// module
	luaL_setfuncs(L, nk_scintilla_lib_methods, 0);
	return 1;
}

