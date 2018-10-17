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

static int metrics_gui_plot_push(lua_State* L) {
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)luaL_checkudata(L, 1, LUA_METRICS_GUI_PLOT_NAME);
	size_t i = (size_t)(luaL_checkinteger(L, 2) - 1);
	float value = (float)luaL_checknumber(L, 3);
	size_t count = ud->count;
	if( i>=0 && i<count ) {
		ud->metrics[i]->AddNewValue(value);
	}
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
	, {"push", metrics_gui_plot_push}
	, {"update", metrics_gui_plot_update}
	, {"draw", metrics_gui_plot_draw}
	, {NULL, NULL}
	};

static int metrics_gui_plot_create(lua_State* L) {
	size_t count = (size_t)luaL_checkinteger(L, 1);
	size_t struct_sz = sizeof(MetricsGuiPlotLua) + sizeof(MetricsGuiPlot*) * count;
	MetricsGuiPlotLua* ud = (MetricsGuiPlotLua*)lua_newuserdata(L, sizeof(MetricsGuiPlotLua) + struct_sz);
	MetricsGuiPlot* plot;
	size_t i;
	memset(ud, 0, struct_sz);
	plot = new MetricsGuiPlot();
	{
		plot->mBarGraph = true;
		plot->mStacked = true;
		plot->mShowAverage = true;
		plot->mShowLegendAverage = true;
		plot->mShowLegendUnits = true;
	}
	for( i=0; i<count; ++i ) {
		char label[64];
		sprintf(label, "Metrics:%d", i);
		MetricsGuiMetric* metric = new MetricsGuiMetric(label, "s", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
		metric->mSelected = true;
		plot->AddMetric(metric);
		ud->metrics[i] = metric;
	}
	plot->SortMetricsByName();
	ud->plot = plot;
	ud->count = count;
	if( luaL_newmetatable(L, LUA_METRICS_GUI_PLOT_NAME) ) {
		luaL_setfuncs(L, metrics_gui_plot_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

