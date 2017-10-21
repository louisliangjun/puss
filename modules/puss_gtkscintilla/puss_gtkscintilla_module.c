// puss_gtkscintilla_module.inl

#include "puss_gobject_module.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <Scintilla.h>
#include <SciLexer.h>
#define GTK
#include <ScintillaWidget.h>

#include "puss_gobject_ffi_reg.h"

static PussGObjectInterface* gobject_iface = NULL;

#define _sci_send(editor, msg, u, s)	scintilla_send_message(SCINTILLA(editor), (msg), (uptr_t)(u), (sptr_t)(s))

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

static int _lua__sci_send_wrap(lua_State* L) {
	IFaceDecl* decl = (IFaceDecl*)lua_touserdata(L, lua_upvalueindex(1));
	ScintillaObject* editor = SCINTILLA(gobject_iface->gobject_check(L, 1));
	int nret = 0;
	uptr_t wparam = 0;
	sptr_t lparam = 0;
	if( !editor )
		return luaL_argerror(L, 1, "convert to type(SCINTILLA) failed!");

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
			int len = (int)_sci_send(editor, decl->message, wparam, lparam);
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

	sptr_t ret = _sci_send(editor, decl->message, wparam, lparam);
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

static void glua_gtkscintilla_register(lua_State* L, PussGObjectRegIface* reg_iface) {
	// vals
	{
		IFaceVal* p;
		for( p=sci_values; p->name; ++p ) {
			lua_pushinteger(L, p->val);
			lua_setfield(L, REG_CONSTS_INDEX, p->name);
		}
	}

// NOTICE : default, not register all sci functions into glua
// 
// #define _USE_GTK_SCINTILLA_REG_TO_GLUA

	// fun/get/set
	reg_iface->push_gtype_index_table(L, SCINTILLA_TYPE_OBJECT, "scintilla");
	{
		IFaceDecl* p;
#ifdef _USE_GTK_SCINTILLA_REG_TO_GLUA
		char name[512];
#endif
		for( p=sci_functions; p->name; ++p ) {
			lua_pushlightuserdata(L, p);
			lua_pushcclosure(L, _lua__sci_send_wrap, 1);
#ifdef _USE_GTK_SCINTILLA_REG_TO_GLUA
			sprintf(buf, "scintilla_%s", name);
			lua_pushvalue(L, -1);
			lua_setfield(L, REG_SYMBOLS_INDEX, name);
#endif
			lua_setfield(L, -2, p->alias);	// use alias only
		}
	}
	lua_pop(L, 1);

	reg_iface->push_gtype_index_table(L, SCINTILLA_TYPE_NOTIFICATION, "scnotification");
	lua_pop(L, 1);	
}

PussInterface* __puss_iface__ = NULL;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	puss_module_require(L, "puss_gtk");
	gobject_iface = puss_interface_check(L, PussGObjectInterface);
	gobject_iface->module_reg(L, glua_gtkscintilla_register);
	gobject_iface->push_master_table(L);
	return 1;
}

