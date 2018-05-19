-- app.lua

-- local puss_editor = puss.import('editor')
local show_imgui_demos = false

__exports.open_file = function(label, filename, line)
	local f = io.open(filename, 'r')
	if not f then return end
	local cxt = f:read('*a')
	f:close()

	-- local ed = puss_editor.create(label, 'lua')
	-- ed:SetText(nil, cxt)
	-- ed:EmptyUndoBuffer()
	-- return ed
end

local function source_editor_main_menu()
	if not imgui.BeginMenuBar() then return end
    if imgui.BeginMenu('Help') then
    	show_imgui_demos = imgui.MenuItem('ImGUI Demos')
        imgui.EndMenu()
    end
    imgui.EndMenuBar()
end

local function source_editor_back_window()
	local flags = ( ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		)
	imgui.SetNextWindowSize(imgui.GetIODisplaySize())
	imgui.Begin("PussBackground", nil, flags)
	source_editor_main_menu()
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

local function source_editor_main_window()
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
	source_editor_back_window()
	source_editor_main_window()

	if show_imgui_demos then
		imgui.ShowDemoWindow()
	end
end

__exports.main = function()
	local w = imgui.ImGuiCreateGLFW("Puss - SourceEditor", 1024, 768)
	while imgui.ImGuiUpdate(w, source_editor_main) do
		imgui.ImGuiRender(w)
	end
	imgui.ImGuiDestroy(w)
end

