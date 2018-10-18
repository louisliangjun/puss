// metrics_gui_lua.inl

#include "metrics_gui/metrics_gui.h"

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
		ud->metrics[i]->mSelected = lua_toboolean(L, 3);
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

