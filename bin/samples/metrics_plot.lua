-- metrics_plot.lua

local curs = { 100, 200, 300 }
local plot = imgui.CreateMetricsGuiPlot(#curs)

__exports.tabs_page_draw = function(page)
	for i=1,3 do
		curs[i] = curs[i] + math.random(-3, 3)
		plot:push(i, curs[i])
	end
	plot:update()
	plot:draw()
	plot:draw(true)
end

