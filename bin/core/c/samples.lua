-- core.c.samples

local tinycc = puss.import('core.tinycc')

local demos = {}

local function samples_update()
	if imgui.Button('load demo1') then demos[1] = tinycc.load_module('samples/demo1.c') end
	if imgui.Button('demo1.foo(1,2)') then print( demos[1].foo(1,2) ) end
	if imgui.Button('demo1.foo(1,2,0)') then print( demos[1].foo(1,2,0) ) end
	if imgui.Button('demo1.bar(999)') then print( demos[1].bar(999) ) end
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(800, 600, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("TCCSamplesWindow", show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	if res then samples_update() end
	imgui.End()
	return show
end
