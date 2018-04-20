// scintilla_imgui_lua.inl

#define LUA_IM_SCI_NAME		"ScintillaIM"

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
	lua_Integer			val;
} IFaceVal;

static int im_scintilla_new(lua_State* L) {
	ScintillaIM** ud;
	ScintillaIM* sci = scintilla_imgui_new();
	if( !sci ) {
		lua_pushnil(L);
		return 0;
	}

	ud = (ScintillaIM**)lua_newuserdata(L, sizeof(ScintillaIM*));
	*ud = sci;
	luaL_setmetatable(L, LUA_IM_SCI_NAME);
	return 1;
}

static int im_scintilla_free(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	ScintillaIM* sci = *ud;
	if( sci ) {
		*ud = NULL;
		scintilla_imgui_free(sci);
	}
	return 0;
}

static int im_scintilla_update(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	scintilla_imgui_update(*ud);
	return 0;
}

static int _lua__sci_send_wrap(lua_State* L) {
	IFaceDecl* decl = (IFaceDecl*)lua_touserdata(L, lua_upvalueindex(1));
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	int lparam_index = (decl->wparam==IFaceType_void) ? 2 : 3;
	int nret = 0;
	uptr_t wparam = 0;
	sptr_t lparam = 0;
	if( !(*ud) )
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
	case IFaceType_bool:	lparam = (sptr_t)lua_toboolean(L, lparam_index);	break;
	case IFaceType_colour:
	case IFaceType_position:
	case IFaceType_int:		lparam = (sptr_t)luaL_checknumber(L, lparam_index);	break;
	case IFaceType_string:	lparam = (sptr_t)luaL_checkstring(L, lparam_index);	break;
	case IFaceType_stringresult:
		{
			int len = (int)scintilla_imgui_send(*ud, decl->message, wparam, lparam);
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

	sptr_t ret = scintilla_imgui_send(*ud, decl->message, wparam, lparam);
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

