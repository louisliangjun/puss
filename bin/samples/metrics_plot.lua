-- metrics_plot.lua

local metrics =
	{ {cur=0, desc='desc1', unit='unit1'}
	, {cur=0, desc='desc2', unit='unit2', use_si_unit_prefix=true}
	, {cur=0, desc='desc3', unit='unit3'}
	, {cur=0, desc='desc4', unit='unit4', min=-100, max=100}
	}
local plot = imgui.CreateMetricsGuiPlot(metrics)

local function FLOAT_OPT(opt, min, max)
	local change, value = imgui.SliderFloat(opt, plot:option(opt), min, max)
	if change then plot:option(opt, value) end
end

local function UINT_OPT(opt, min, max)
	local change, value = imgui.SliderInt(opt, plot:option(opt), min, max)
	if change then plot:option(opt, value) end
end

local function BOOL_OPT(opt)
	local change, value = imgui.Checkbox(opt, plot:option(opt))
	if change then plot:option(opt, value) end
end

__exports.tabs_page_draw = function(page)
	-- options
	FLOAT_OPT('mBarRounding', 0, 0.5*imgui.GetTextLineHeight(), '%.1f')         -- amount of rounding on bars
	FLOAT_OPT('mRangeDampening', 0, 1, '%.2f')      -- weight of historic range on axis range [0,1]
	UINT_OPT('mInlinePlotRowCount', 1, 10)   -- height of DrawList('') inline plots, in text rows
	UINT_OPT('mPlotRowCount', 1, 10)         -- height of DrawHistory('') plots, in text rows
	UINT_OPT('mVBarMinWidth', 1, 20)         -- min width of bar graph bar in pixels
	UINT_OPT('mVBarGapWidth', 0, 10)         -- width of bar graph inter-bar gap in pixels
	BOOL_OPT('mShowAverage')          -- draw horizontal line at series average
	BOOL_OPT('mShowInlineGraphs')     -- show history plot in DrawList('')
	BOOL_OPT('mShowOnlyIfSelected')   -- draw show selected metrics
	BOOL_OPT('mShowLegendDesc')       -- show series description in legend
	BOOL_OPT('mShowLegendColor')      -- use series color in legend
	BOOL_OPT('mShowLegendUnits')      -- show units in legend values
	BOOL_OPT('mShowLegendAverage')    -- show series average in legend
	BOOL_OPT('mShowLegendMin')        -- show plot y-axis minimum in legend
	BOOL_OPT('mShowLegendMax')        -- show plot y-axis maximum in legend
	BOOL_OPT('mBarGraph')             -- use bars to draw history
	BOOL_OPT('mStacked')              -- stack series when drawing history
	BOOL_OPT('mSharedAxis')           -- use first series' axis range
	BOOL_OPT('mFilterHistory')        -- allow single plot point to represent more than on history value

	-- select
	imgui.Text('select:')
	for i,metric in ipairs(metrics) do
		imgui.SameLine()
		local change, value = imgui.Checkbox(metric.desc, plot:select(i))
		if change then plot:select(i, value) end
	end

	-- demo data
	for i,metric in ipairs(metrics) do
		local v = metric.cur + math.random(-3, 3)
		metric.cur = v
		plot:push(i, v)
	end
	plot:update()

	-- draw
	plot:draw()
	plot:draw(true)
end

