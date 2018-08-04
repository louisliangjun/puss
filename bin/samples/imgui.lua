-- nuklear.lua

local show_lua_window = true

local function imgui_demo_lua()
	-- imgui.ShowUserGuide()
	imgui.ShowDemoWindow()
	if not show_lua_window then return end
	ok, show_lua_window = imgui.Begin("Lua Window", show_lua_window)
	imgui.Text("Hello from lua window!")
	if puss.debug and imgui.Button("start debugger") then
		puss.debug()
	end
	if imgui.Button("close") then
		show_lua_window = false
	end
	imgui.End()
end

function __main__()
	local w = imgui.Create("imgui lua api", 1024, 768)
	while not w:should_close() do
		w(imgui_demo_lua)
	end
end

if not imgui then
	_ENV.imgui = puss.require('puss_imgui')
	--_ENV.imgui = puss.require('puss_imgui_glfw')
	puss.dofile(puss._script)	-- for use nk symbols & enums
end
