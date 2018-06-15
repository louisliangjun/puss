// scintilla_imgui_lua.inl

#define LUA_IM_SCI_NAME		"ScintillaIM"
#define LUA_IM_SCI_LEXERS	"ScintillaLexers"

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

typedef struct _IFaceLex {
	const char*			name;
	lua_Integer			lexer;
} IFaceLex;

static int im_scintilla_create(lua_State* L) {
	ScintillaIM** ud;
	ScintillaIM* sci = scintilla_imgui_create();
	if( !sci ) {
		lua_pushnil(L);
		return 0;
	}

	ud = (ScintillaIM**)lua_newuserdata(L, sizeof(ScintillaIM*));
	*ud = sci;
	luaL_setmetatable(L, LUA_IM_SCI_NAME);
	lua_newtable(L);
	lua_setuservalue(L, -2);
	return 1;
}

static int im_scintilla_destroy(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	ScintillaIM* sci = *ud;
	if( sci ) {
		*ud = NULL;
		scintilla_imgui_destroy(sci);
	}
	return 0;
}

static int _lua__sci_send_wrap(lua_State* L) {
	IFaceDecl* decl = (IFaceDecl*)lua_touserdata(L, lua_upvalueindex(1));
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	int lparam_index = (decl->wparam==IFaceType_void) ? 2 : 3;
	int nret = 0;
	uptr_t wparam = 0;
	sptr_t lparam = 0;
	Sci_TextToFind ft;
	if( !(*ud) )
		return luaL_argerror(L, 1, "ScintillaNK already free!");

	switch( decl->wparam ) {
	case IFaceType_void:	break;
	case IFaceType_bool:	wparam = (uptr_t)lua_toboolean(L, 2);		break;
	case IFaceType_colour:
	case IFaceType_position:
	case IFaceType_int:		wparam = (uptr_t)luaL_checkinteger(L, 2);	break;
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
	case IFaceType_int:		lparam = (sptr_t)luaL_checkinteger(L, lparam_index);	break;
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
	case IFaceType_findtext:
		{
			lparam = (sptr_t)&ft;
			ft.lpstrText = luaL_checkstring(L, lparam_index);
			ft.chrg.cpMin = (Sci_PositionCR)luaL_optinteger(L, lparam_index+1, 0);
			ft.chrg.cpMax = lua_isinteger(L, lparam_index+2)
				? (Sci_PositionCR)lua_tointeger(L,lparam_index+2)
				: (Sci_PositionCR)scintilla_imgui_send(*ud, SCI_GETLENGTH, 0, 0)
				;
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
	case IFaceType_int:		lua_pushinteger(L, (lua_Integer)ret);	++nret;	break;
	default:
		if( decl->rtype < IFaceTypeMax )
			return luaL_error(L, "not support return type(%s)", iface_type_name[decl->rtype]);
		return luaL_error(L, "not support return type(%d) system error!", decl->rtype);
	}

	switch( decl->lparam ) {
	case IFaceType_stringresult:
		lua_pushlstring(L, (const char*)lparam, (size_t)ret);
		++nret;
		break;
	case IFaceType_findtext:
		if( ((int)ret) != -1 ) {
			lua_pushinteger(L, ft.chrgText.cpMin);
			lua_pushinteger(L, ft.chrgText.cpMax);
			nret += 2;
		}
		break;
	default:
		break;
	}

	return nret;
}

static void im_scintilla_on_notify(lua_State* L, ImguiEnv* env, ScintillaIM* sci, const SCNotification* ev) {
	if( lua_getuservalue(L, 1)!=LUA_TTABLE )
		return;
	int top = lua_gettop(L);
	if( lua_rawgeti(L, -1, ev->nmhdr.code)!=LUA_TFUNCTION )
		return;
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_replace(L, top);
	lua_pushvalue(L, 1);
	switch(ev->nmhdr.code) {
	case SCN_STYLENEEDED:	// void StyleNeeded=2000(int position)
		lua_pushinteger(L, ev->position);
		break;
	case SCN_CHARADDED:		// void CharAdded=2001(int ch)
		lua_pushinteger(L, ev->ch);
		break;
	case SCN_KEY:	// void Key=2005(int ch, int modifiers)
		lua_pushinteger(L, ev->ch);
		lua_pushinteger(L, ev->modifiers);
		break;
	case SCN_DOUBLECLICK:	// void DoubleClick=2006(int modifiers, int position, int line)
		lua_pushinteger(L, ev->modifiers);
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->line);
		break;
	case SCN_UPDATEUI:	// void UpdateUI=2007(int updated)
		lua_pushinteger(L, ev->updated);
		break;
	case SCN_MODIFIED:	// void Modified=2008(int position, int modificationType, string text, int length, int linesAdded, int line, int foldLevelNow, int foldLevelPrev, int token, int annotationLinesAdded)
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->modificationType);
		lua_pushstring(L, ev->text);
		lua_pushinteger(L, ev->length);
		lua_pushinteger(L, ev->linesAdded);
		lua_pushinteger(L, ev->line);
		lua_pushinteger(L, ev->foldLevelNow);
		lua_pushinteger(L, ev->foldLevelPrev);
		lua_pushinteger(L, ev->token);
		lua_pushinteger(L, ev->annotationLinesAdded);
		break;
	case SCN_MACRORECORD:	// void MacroRecord=2009(int message, int wParam, int lParam)
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->wParam);
		lua_pushinteger(L, ev->lParam);
		break;
	case SCN_NEEDSHOWN:	// void NeedShown=2011(int position, int length)
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->length);
		break;
	case SCN_USERLISTSELECTION:	// void UserListSelection=2014(int listType, string text, int position, int ch, CompletionMethods listCompletionMethod)
		lua_pushinteger(L, ev->listType);
		lua_pushstring(L, ev->text);
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->ch);
		lua_pushinteger(L, ev->listCompletionMethod);
		break;
	case SCN_URIDROPPED:	// void URIDropped=2015(string text)
		lua_pushstring(L, ev->text);
		break;
	case SCN_DWELLSTART:	// void DwellStart=2016(int position, int x, int y)
	case SCN_DWELLEND:	// void DwellEnd=2017(int position, int x, int y)
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->x);
		lua_pushinteger(L, ev->y);
		break;
	case SCN_HOTSPOTCLICK:	// void HotSpotClick=2019(int modifiers, int position)
	case SCN_HOTSPOTDOUBLECLICK:	// void HotSpotDoubleClick=2020(int modifiers, int position)
	case SCN_INDICATORCLICK:	// void IndicatorClick=2023(int modifiers, int position)
	case SCN_INDICATORRELEASE:	// void IndicatorRelease=2024(int modifiers, int position)
	case SCN_HOTSPOTRELEASECLICK:	// void HotSpotReleaseClick=2027(int modifiers, int position)
		lua_pushinteger(L, ev->modifiers);
		lua_pushinteger(L, ev->position);
		break;
	case SCN_CALLTIPCLICK:	// void CallTipClick=2021(int position)
		lua_pushinteger(L, ev->position);
		break;
	case SCN_AUTOCSELECTION:	// void AutoCSelection=2022(string text, int position, int ch, CompletionMethods listCompletionMethod)
	case SCN_AUTOCCOMPLETED:	// void AutoCCompleted=2030(string text, int position, int ch, CompletionMethods listCompletionMethod)
		lua_pushstring(L, ev->text);
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->ch);
		lua_pushinteger(L, ev->listCompletionMethod);
		break;
	case SCN_MARGINCLICK:// void MarginClick=2010(int modifiers, int position, int margin)
	case SCN_MARGINRIGHTCLICK:	// void MarginRightClick=2031(int modifiers, int position, int margin)
		lua_pushinteger(L, ev->modifiers);
		lua_pushinteger(L, ev->position);
		lua_pushinteger(L, ev->margin);
		break;
	case SCN_AUTOCSELECTIONCHANGE:	// void AutoCSelectionChange=2032(int listType, string text, int position)
		lua_pushinteger(L, ev->listType);
		lua_pushstring(L, ev->text);
		lua_pushinteger(L, ev->position);
		break;
	// case SCN_SAVEPOINTREACHED:	// void SavePointReached=2002(void)
	// case SCN_SAVEPOINTLEFT:	// void SavePointLeft=2003(void)
	// case SCN_MODIFYATTEMPTRO:	// void ModifyAttemptRO=2004(void)
	// case SCN_PAINTED:	// void Painted=2013(void)
	// case SCN_ZOOM:	// void Zoom=2018(void)
	// case SCN_AUTOCCANCELLED:	// void AutoCCancelled=2025(void)
	// case SCN_AUTOCCHARDELETED:	// void AutoCCharDeleted=2026(void)
	// case SCN_FOCUSIN:	// void FocusIn=2028(void)
	// case SCN_FOCUSOUT:	// void FocusOut=2029(void)
	default:
		break;
	}
	lua_pcall(L, lua_gettop(L) - top - 1, 0, top);
}

static void im_scintilla_update_callback(ScintillaIM* sci, const SCNotification* ev, void* ud) {
	lua_State* L = (lua_State*)ud;
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData);
	if( !env )	return;
	if( ev ) {
		if( luaL_testudata(L, 1, LUA_IM_SCI_NAME) ) {
			int top = lua_gettop(L);
			im_scintilla_on_notify(L, env, sci, ev);
			lua_settop(L, top);
		}
	} else {
		if( lua_isfunction(L, 3) ) {
			lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
			lua_replace(L, 2);
			lua_pushvalue(L, 1);	// self
			lua_insert(L, 4);
			lua_pcall(L, lua_gettop(L)-3, 0, 2);
		}
	}
}

static int im_scintilla_update(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	bool draw = lua_toboolean(L, 2);
	if( !draw ) luaL_checktype(L, 3, LUA_TFUNCTION);
	scintilla_imgui_update(*ud, draw, im_scintilla_update_callback, L);
	return 0;
}

static int im_scintilla_get_data(lua_State* L) {
	luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	luaL_argcheck(L, !lua_isnoneornil(L, 2), 2, "key MUST exist");
	lua_getuservalue(L, 1);
	lua_pushvalue(L, 2);
	lua_gettable(L, -2);
	return 1;
}

static int im_scintilla_set_data(lua_State* L) {
	luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	luaL_argcheck(L, !lua_isnoneornil(L, 2), 2, "key MUST exist");
	lua_pushnil(L);
	lua_getuservalue(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, 3);
	lua_settable(L, -3);
	return 0;
}

static int im_scintilla_get_lexers(lua_State* L, IFaceLex* lexers) {
	if( lua_getfield(L, LUA_REGISTRYINDEX, LUA_IM_SCI_LEXERS)==LUA_TTABLE )
		return 1;
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_IM_SCI_LEXERS);
	for( IFaceLex* p=lexers; p->name; ++p ) {
		lua_pushinteger(L, p->lexer);
		lua_setfield(L, -2, p->name);
	}
	return 1;
}

