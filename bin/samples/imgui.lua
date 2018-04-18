-- nuklear.lua

local show_lua_window = true
local function imgui_demo_lua(ctx, sci)
	-- simgui.ShowUserGuide()
	imgui.ShowDemoWindow()
	if not show_lua_window then return end
	ok, show_lua_window = imgui.Begin("Lua Window", show_lua_window)
	imgui.Text("Hello from lua window!")
	if imgui.Button("close") then
		show_lua_window = false
	end
	imgui.scintilla_update(sci, ctx)
	imgui.End()
end

function __main__()
	local w = imgui.glfw_imgui_create("imgui lua api", 1024, 768)
	local sci = imgui.scintilla_new()
	sci:SetText("abcde中文fg")
	while w:update(imgui_demo_lua, 0.001, sci) do
		w:render()
	end
	w:destroy()
end

if not imgui then
	_ENV.imgui = puss.require('puss_imgui')
	puss.dofile(puss._script)	-- for use nk symbols & enums
end
