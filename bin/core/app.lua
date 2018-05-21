-- app.lua

local console = puss.import('core.console')

local show_imgui_demos = false
local show_console_window = false

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
    if imgui.BeginMenu('Help') then
    	active, show_imgui_demos = imgui.MenuItem('ImGUI Demos', nil, show_imgui_demos)
    	active, show_console_window = imgui.MenuItem('Conosle', nil, show_console_window)
    	if active then console = puss.import('core.console', true) end
        imgui.EndMenu()
    end
    imgui.EndMenuBar()
end

local function back_window()
	local flags = ( ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		)
	imgui.SetNextWindowSize(imgui.GetIODisplaySize())
	imgui.Begin("PussBackground", nil, flags)
	main_menu()
	if imgui.Button("111") then
	end
	imgui.SameLine()
	if imgui.Button("222") then
	end
	imgui.SameLine()
	if imgui.Button("333") then
	end
	imgui.End()
end

local function main_window()
	imgui.Begin("Puss Main Window")
	if imgui.Button("111") then
	end
	imgui.SameLine()
	if imgui.Button("222") then
	end
	imgui.SameLine()
	if imgui.Button("333") then
	end
	imgui.End()
end

local function source_editor_main()
	back_window()
	main_window()
	local active
	if show_imgui_demos then imgui.ShowDemoWindow() end
	if show_console_window then console.update() end
end

__exports.main = function()
	local w = imgui.ImGuiCreateGLFW("Puss - Editor", 1024, 768)
	while imgui.ImGuiUpdate(w, source_editor_main) do
		imgui.ImGuiRender(w)
	end
	imgui.ImGuiDestroy(w)
end

