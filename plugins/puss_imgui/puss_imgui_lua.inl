// puss_imgui_lua.inl

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "imgui.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "puss_plugin.h"

#include "metrics_gui/metrics_gui.h"
#include "scintilla_imgui.h"

#if defined(__GNUC__)
	#pragma GCC diagnostic ignored "-Wunused-function"          // warning: 'xxxx' defined but not used
#endif

enum PussImGuiKeyType
	{ PUSS_IMGUI_BASIC_KEY_LAST = 255
#define _PUSS_IMGUI_KEY_REG(key)	, PUSS_IMGUI_KEY_ ## key
	#include "puss_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG
	, PUSS_IMGUI_TOTAL_KEY_LAST
	};

static ImVec2					g_DropPos;
static ImVector<char>           g_DropFiles;
static lua_State*				g_UpdateLuaState = NULL;
static int                      g_ScriptErrorHandle = LUA_NOREF;
static int                      g_StackProtected = 0;
static ImVector<char>           g_Stack;

static inline void _wrap_stack_begin(char tp) {
	g_Stack.push_back(tp);
}

static inline void _wrap_stack_end(char tp) {
	int pe = g_Stack.size();
	int ps = g_StackProtected < 0 ? 0 : g_StackProtected;
	for( int i=pe-1; i>=ps; --i ) {
		if( g_Stack[i]==tp ) {
			g_Stack.erase(&g_Stack[i]);
			return;
		}
	}
	IM_ASSERT(0 && "stack pop type not matched!");
}

int puss_imgui_assert_hook(const char* expr, const char* file, int line) {
	if( g_UpdateLuaState ) {
		luaL_error(g_UpdateLuaState, "%s @%s:%d", expr, file, line);
	} else {
		fprintf(stderr, "%s @%s:%d\r\n", expr, file, line);
	}
	return 0;
}

#define IMGUI_LUA_WRAP_STACK_BEGIN(tp)	_wrap_stack_begin(tp);
#define IMGUI_LUA_WRAP_STACK_END(tp)	_wrap_stack_end(tp);
#include "imgui_lua.inl"
#undef IMGUI_LUA_WRAP_STACK_BEGIN
#undef IMGUI_LUA_WRAP_STACK_END

#define IMGUI_LIB_NAME	"ImguiLib"

static int imgui_error_handle_default(lua_State* L) {
	fprintf(stderr, "[ImGui] error: %s\n", lua_tostring(L, -1));
	return lua_gettop(L);
}

static void imgui_error_handle_push(lua_State* L) {
	if( g_ScriptErrorHandle!=LUA_NOREF ) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, g_ScriptErrorHandle);
	} else {
		lua_pushcfunction(L, imgui_error_handle_default);
	}
}

static int imgui_set_error_handle_lua(lua_State* L) {
	if( lua_isfunction(L, 1) ) {
		lua_pushvalue(L, 1);
		if( g_ScriptErrorHandle==LUA_NOREF ) {
			g_ScriptErrorHandle = luaL_ref(L, LUA_REGISTRYINDEX);
		} else {
			lua_rawseti(L, LUA_REGISTRYINDEX, g_ScriptErrorHandle);
		}
	}
	if( g_ScriptErrorHandle!=LUA_NOREF ) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, g_ScriptErrorHandle);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int imgui_protect_pcall_lua(lua_State* L) {
	int base = g_StackProtected;
	int top = g_Stack.size();
	g_StackProtected = top;
	imgui_error_handle_push(L);
	lua_insert(L, 1);
	lua_pushboolean(L, lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1)==LUA_OK);
	lua_replace(L, 1);
	g_StackProtected = base;
	while( g_Stack.size() > top ) {
		int tp = g_Stack.back();
		g_Stack.pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}
	return lua_gettop(L);
}

static int imgui_getio_delta_time_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	lua_pushnumber(L, ctx ? ctx->IO.DeltaTime : 0.0f);
	return 1;
}

static bool shortcut_mod_check(lua_State* L, int arg, bool val) {
	return lua_isnoneornil(L, arg) ? true : (val == ((lua_toboolean(L, arg)!=0)));
}

static int imgui_is_shortcut_pressed_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	int key = (int)luaL_checkinteger(L, 1);
	bool pressed = ctx
		&& shortcut_mod_check(L, 2, ctx->IO.KeyCtrl)
		&& shortcut_mod_check(L, 3, ctx->IO.KeyShift)
		&& shortcut_mod_check(L, 4, ctx->IO.KeyAlt)
		&& shortcut_mod_check(L, 5, ctx->IO.KeySuper)
		&& (key>=0 && key<PUSS_IMGUI_TOTAL_KEY_LAST)
		&& ImGui::IsKeyPressed(key)
		;
	lua_pushboolean(L, pressed ? 1 : 0);
	return 1;
}

static int imgui_fetch_extra_keys_lua(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
#define _PUSS_IMGUI_KEY_REG(key)	lua_pushinteger(L, PUSS_IMGUI_KEY_ ## key);	lua_setfield(L, 1, #key);
	#include "puss_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG
	return 0;
}

static int imgui_get_drop_files_lua(lua_State* L) {
	int check_drop_pos_in_window = lua_toboolean(L, 1);
	if( g_DropFiles.empty() )
		return 0;
	if( check_drop_pos_in_window ) { 
		ImGuiWindow* win = ImGui::GetCurrentWindowRead();
		if( !(win && win->Rect().Contains(g_DropPos)) )
			return 0;
	}
	lua_pushlstring(L, g_DropFiles.Data, g_DropFiles.Size);
	return 1;
}

static int add_ttf_font_file_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	const char* fname = luaL_checkstring(L, 1);
	float size_pixel = (float)luaL_checknumber(L, 2);
	const char* language = luaL_optstring(L, 3, NULL);
	const ImWchar* glyph_ranges = NULL;
	ImFont* font = NULL;
	if( !ctx )	luaL_error(L, "ImGui::GetCurrentContext MUST exist!");
	if( language ) {
		if( strcmp(language, "Korean")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesKorean();
		} else if( strcmp(language, "Japanese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesJapanese();
		} else if( strcmp(language, "Japanese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesJapanese();
		} else if( strcmp(language, "Chinese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesChineseFull();
		} else if( strcmp(language, "ChineseSimplified")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon();
		} else if( strcmp(language, "Cyrillic")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesCyrillic();
		} else if( strcmp(language, "Thai")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesThai();
		} else {
			luaL_error(L, "Not support font language: %s", language);
		}
	}
	font = ctx->IO.Fonts->AddFontFromFileTTF(fname, size_pixel, NULL, glyph_ranges);
	lua_pushboolean(L, font ? 1 : 0);
	return 1;
}

#define LUA_METRICS_GUI_PLOT_NAME		"MetricsGuiPlot"

typedef struct MetricsGuiPlotLua {
	MetricsGuiPlot*		plot;
	size_t				count;
	MetricsGuiMetric*	metrics[1];
} MetricsGuiPlotLua;

static int metrics_gui_plot_destroy(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	size_t count = ud->count;
	size_t i;
	ud->count = 0;
	for( i=0; i<count; ++i ) {
		delete ud->metrics[i];
		ud->metrics[i] = NULL;
	}
	if( ud->plot ) {
		delete ud->plot;
		ud->plot = NULL;
	}
	return 0;
}

static int metrics_gui_plot_select(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	size_t i = (size_t)(luaL_checkinteger(L, 2) - 1);
	size_t count = ud->count;
	bool change = !(lua_isnoneornil(L, 3));
	luaL_argcheck(L, i<count, 2, "out of range");
	lua_pushboolean(L, ud->metrics[i]->mSelected ? 1 : 0);
	if( change ) {
		ud->metrics[i]->mSelected = lua_toboolean(L, 3) ? true : false;
	}
	return 1;
}

static int metrics_gui_plot_push(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	size_t i = (size_t)(luaL_checkinteger(L, 2) - 1);
	float value = (float)luaL_checknumber(L, 3);
	size_t count = ud->count;
	luaL_argcheck(L, i<count, 2, "out of range");
	ud->metrics[i]->AddNewValue(value);
	return 0;
}

static int metrics_gui_plot_last(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	size_t i = (size_t)(luaL_checkinteger(L, 2) - 1);
	uint32_t prev = (uint32_t)(luaL_optinteger(L, 4, 0));
	size_t count = ud->count;
	float old;
	luaL_argcheck(L, i<count, 2, "index out of range");
	luaL_argcheck(L, prev<MetricsGuiMetric::NUM_HISTORY_SAMPLES, 2, "prev out of range");
	old = ud->metrics[i]->GetLastValue(prev);
	if( lua_isnumber(L, 3) ) {
		ud->metrics[i]->SetLastValue((float)lua_tonumber(L, 3));
	}
	lua_pushnumber(L, old);
	return 1;
}

static int metrics_gui_plot_option(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	const char* opt = luaL_checkstring(L, 2);
	int readonly = lua_isnoneornil(L, 3);
	if( !ud->plot ) return 0;

	#define FLOAT_OPT(option)	if( strcmp(opt, #option)==0 ) { lua_pushnumber(L, ud->plot-> option); if(!readonly) { ud->plot-> option = (float)luaL_checknumber(L, 3); } return 1; }
	#define UINT_OPT(option)	if( strcmp(opt, #option)==0 ) { lua_pushinteger(L, (lua_Integer)(ud->plot-> option)); if(!readonly) { ud->plot-> option = (uint32_t)luaL_checkinteger(L, 3); } return 1; }
	#define BOOL_OPT(option)	if( strcmp(opt, #option)==0 ) { lua_pushboolean(L, ud->plot-> option ? 1 : 0); if(!readonly) { ud->plot-> option = lua_toboolean(L, 3) ? true : false; } return 1; }

	FLOAT_OPT(mBarRounding)         // amount of rounding on bars
    FLOAT_OPT(mRangeDampening)      // weight of historic range on axis range [0,1]
    UINT_OPT(mInlinePlotRowCount)   // height of DrawList() inline plots, in text rows
    UINT_OPT(mPlotRowCount)         // height of DrawHistory() plots, in text rows
    UINT_OPT(mVBarMinWidth)         // min width of bar graph bar in pixels
    UINT_OPT(mVBarGapWidth)         // width of bar graph inter-bar gap in pixels
    BOOL_OPT(mShowAverage)          // draw horizontal line at series average
    BOOL_OPT(mShowInlineGraphs)     // show history plot in DrawList()
    BOOL_OPT(mShowOnlyIfSelected)   // draw show selected metrics
    BOOL_OPT(mShowLegendDesc)       // show series description in legend
    BOOL_OPT(mShowLegendColor)      // use series color in legend
    BOOL_OPT(mShowLegendUnits)      // show units in legend values
    BOOL_OPT(mShowLegendAverage)    // show series average in legend
    BOOL_OPT(mShowLegendMin)        // show plot y-axis minimum in legend
    BOOL_OPT(mShowLegendMax)        // show plot y-axis maximum in legend
    BOOL_OPT(mBarGraph)             // use bars to draw history
    BOOL_OPT(mStacked)              // stack series when drawing history
    BOOL_OPT(mSharedAxis)           // use first series' axis range
    BOOL_OPT(mFilterHistory)        // allow single plot point to represent more than on history value

	#undef FLOAT_OPT
	#undef UINT_OPT
	#undef BOOL_OPT
 
	return 0;
}

static int metrics_gui_plot_update(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	if( ud->plot ) {
		ud->plot->UpdateAxes();
	}
	return 0;
}

static int metrics_gui_plot_draw(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	int darw_histroy = lua_toboolean(L, 2);
	if( ud->plot ) {
		if( darw_histroy ) {
			ud->plot->DrawHistory();
		} else {
			ud->plot->DrawList();
		}
	}
	return 0;
}

static luaL_Reg metrics_gui_plot_methods[] =
	{ {"__gc", metrics_gui_plot_destroy}
	, {"select", metrics_gui_plot_select}
	, {"push", metrics_gui_plot_push}
	, {"last", metrics_gui_plot_last}
	, {"option", metrics_gui_plot_option}
	, {"update", metrics_gui_plot_update}
	, {"draw", metrics_gui_plot_draw}
	, {NULL, NULL}
	};

typedef struct MetricDesc {
	MetricDesc*	next;
	const char*	desc;
	const char*	unit;
	unsigned	flag;
	float		known_min;
	float		known_max;
} MetricDesc;

static const char* parse_metric_str_field(lua_State* L, const char* field, const char* def) {
	const char* v = lua_getfield(L, -1, field)==LUA_TSTRING ? lua_tostring(L, -1) : def;
	lua_pop(L, 1);
	return v;
}

static bool parse_metric_num_field(lua_State* L, const char* field, float& v) {
	bool ok = (lua_getfield(L, -1, field)==LUA_TNUMBER);
	v = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
	return ok;
}

static bool parse_metric_bool_field(lua_State* L, const char* field) {
	bool v = false;
	lua_getfield(L, -1, field);
	v = lua_toboolean(L, -1) ? true : false;
	lua_pop(L, 1);
	return v;
}

static size_t parse_metric_descs(lua_State* L, MetricDesc** pnext) {
	MetricDesc* cur = NULL;
	lua_Integer count = 0;
	lua_Integer i;
	luaL_checktype(L, 1, LUA_TTABLE);
	count = luaL_len(L, 1);
	luaL_argcheck(L, count>0, 2, "metrics can not empty");
	for( i=1; i<=count; ++i ) {
		lua_geti(L, 1, i);
		luaL_argcheck(L, lua_istable(L, -1), 1, "metrics info MUST table");
		cur = (MetricDesc*)lua_newuserdata(L, sizeof(MetricDesc));
		lua_setfield(L, -2, "__desc__");
		*pnext = cur;
		pnext = &(cur->next);
		cur->next = NULL;
		cur->desc = parse_metric_str_field(L, "desc", "");
		cur->unit = parse_metric_str_field(L, "unit", "");
		cur->flag = MetricsGuiMetric::NONE;
		cur->known_min = 0.0f;
		cur->known_max = 0.0f;
		if( parse_metric_bool_field(L, "use_si_unit_prefix") )
			cur->flag |= MetricsGuiMetric::USE_SI_UNIT_PREFIX;
		if( parse_metric_num_field(L, "min", cur->known_min) )
			cur->flag |= MetricsGuiMetric::KNOWN_MIN_VALUE;
		if( parse_metric_num_field(L, "max", cur->known_max) )
			cur->flag |= MetricsGuiMetric::KNOWN_MAX_VALUE;
		lua_pop(L, 1);
	}
	return count;
}

static int metrics_gui_plot_create(lua_State* L) {
	MetricDesc* info = NULL;
	size_t count = parse_metric_descs(L, &info);
	size_t struct_sz = sizeof(MetricsGuiPlotLua) + sizeof(MetricsGuiPlot*) * count;
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)lua_newuserdata(L, sizeof(MetricsGuiPlotLua) + struct_sz);
	size_t i;
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_argcheck(L, count<256, 2, "index out of range");
	memset(ud, 0, struct_sz);
	ud->count = count;
	for( i=0; i<count; ++i ) {
		MetricsGuiMetric* metric = new MetricsGuiMetric(info->desc, info->unit, info->flag);
		metric->mKnownMinValue = info->known_min;
		metric->mKnownMaxValue = info->known_max;
		ud->metrics[i] = metric;
		info = info->next;
	}
	ud->plot = new MetricsGuiPlot();
	for( i=0; i<count; ++i ) {
		ud->plot->AddMetric(ud->metrics[i]);
	}
	ud->plot->SortMetricsByName();
	if( luaL_newmetatable(L, LUA_METRICS_GUI_PLOT_NAME) ) {
		luaL_setfuncs(L, metrics_gui_plot_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

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

#include "scintilla.iface.inl"

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

static void im_scintilla_on_notify(lua_State* L, ScintillaIM* sci, const SCNotification* ev, int error_handle) {
	int top = lua_gettop(L);
	if( lua_getuservalue(L, 1)!=LUA_TTABLE )
		return;
	if( lua_rawgeti(L, -1, ev->nmhdr.code)!=LUA_TFUNCTION )
		return;
	lua_remove(L, -2);
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
	lua_pcall(L, lua_gettop(L) - top - 1, 0, error_handle);
}

static void im_scintilla_update_callback(ScintillaIM* sci, const SCNotification* ev, void* ud) {
	lua_State* L = (lua_State*)ud;
	if( ev ) {
		if( luaL_testudata(L, 1, LUA_IM_SCI_NAME) ) {
			int top = lua_gettop(L);
			imgui_error_handle_push(L);
			im_scintilla_on_notify(L, sci, ev, top+1);
			lua_settop(L, top);
		}
	} else if( lua_isfunction(L, 2) ) {
		lua_pushvalue(L, 1);	// self
		imgui_error_handle_push(L);
		lua_replace(L, 1);
		lua_insert(L, 3);
		lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1);
	}
}

static int im_scintilla_update(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	scintilla_imgui_update(*ud, lua_isfunction(L,2) ? false : true, im_scintilla_update_callback, L);
	return lua_gettop(L) - 1;
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

static int im_scintilla_lexers(lua_State* L) {
	if( lua_getfield(L, LUA_REGISTRYINDEX, LUA_IM_SCI_LEXERS)==LUA_TTABLE )
		return 1;
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_IM_SCI_LEXERS);
	for( IFaceLex* p=sci_lexers; p->name; ++p ) {
		lua_pushinteger(L, p->lexer);
		lua_setfield(L, -2, p->name);
	}
	return 1;
}

static luaL_Reg imgui_lua_apis[] =
	{ {"set_error_handle", imgui_set_error_handle_lua}
	, {"protect_pcall", imgui_protect_pcall_lua}

	, {"GetDropFiles", imgui_get_drop_files_lua}
	, {"AddFontFromFileTTF", add_ttf_font_file_lua}

	, {"CreateByteArray", byte_array_create}
	, {"CreateFloatArray", float_array_create}
	, {"CreateDockFamily", dock_family_create}
	, {"CreateScintilla", im_scintilla_create}
	, {"CreateMetricsGuiPlot", metrics_gui_plot_create}
	, {"GetScintillaLexers", im_scintilla_lexers}

	, {"GetIODeltaTime", imgui_getio_delta_time_lua}
	, {"IsShortcutPressed", imgui_is_shortcut_pressed_lua}
	, {"FetchExtraKeys", imgui_fetch_extra_keys_lua}

#define __REG_WRAP(w,f)	, { #w, f }
	#include "imgui_wraps.inl"
#undef __REG_WRAP
	, {NULL, NULL}
	};

static void lua_register_imgui_consts(lua_State* L) {
	// puss imgui keys
#define _PUSS_IMGUI_KEY_REG(key)	lua_pushinteger(L, PUSS_IMGUI_KEY_ ## key);	lua_setfield(L, -2, "PUSS_IMGUI_KEY_" #key);
	#include "puss_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG

	// imgui enums
#define __REG_ENUM(e)	lua_pushinteger(L, e);	lua_setfield(L, -2, #e);
	#include "imgui_enums.inl"
#undef __REG_ENUM

	// sci enums
	{
		for( IFaceVal* p=sci_values; p->name; ++p ) {
			lua_pushinteger(L, p->val);
			lua_setfield(L, -2, p->name);
		}
	}
}

static void lua_register_scintilla(lua_State* L) {
	// metatable: fun/get/set
	if( luaL_newmetatable(L, LUA_IM_SCI_NAME) ) {
		static luaL_Reg methods[] =
			{ {"__index", NULL}
			, {"__call",im_scintilla_update}
			, {"__gc",im_scintilla_destroy}
			, {"destroy",im_scintilla_destroy}
			, {"set",im_scintilla_set_data}
			, {"get",im_scintilla_get_data}
			, {NULL, NULL}
			};
		luaL_setfuncs(L, methods, 0);

		for( IFaceDecl* p=sci_functions; p->name; ++p ) {
			lua_pushlightuserdata(L, p);
			lua_pushcclosure(L, _lua__sci_send_wrap, 1);
			lua_setfield(L, -2, p->name);
		}
	}
	lua_setfield(L, -1, "__index");
}

