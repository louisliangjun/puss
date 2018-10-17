-- plot.lua

local arr = imgui.CreateFloatArray(256)
local cur = 100

__exports.tabs_page_draw = function(page)
	cur = cur + math.random(-3, 3)
	arr:push(cur)
	imgui.PlotLines('Demo', arr, 256, 0, 'overlay', nil, nil, 0, 100)
	imgui.PlotHistogram('Demo2', arr, 128, nil, 'overlay2', nil, nil, 0, 100)
end

