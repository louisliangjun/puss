-- app.lua

local shotcuts = puss.import('core.shotcuts')
local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local demos = puss.import('core.demos')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')
local diskfs = puss.import('core.diskfs')

filebrowser.setup(diskfs)
docs.setup(diskfs)

local run_sign = true
local main_ui = _main_ui

show_imgui_demos = show_imgui_demos or false
show_tabs_demo = show_tabs_demo or false
show_console_window = show_console_window or false
show_shutcut_window = show_shutcut_window or false

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
	if imgui.BeginMenu('File') then
		if imgui.MenuItem('Add puss path FileBrowser') then filebrowser.append_folder(puss.local_to_utf8(puss._path)) end
		if imgui.MenuItem('Add Demo Tab') then demos.new_page() end
		if imgui.MenuItem('New page') then docs.new_page() end
		if imgui.MenuItem('Open app.lua') then docs.open(puss._path .. '/core/app.lua') end
		if imgui.MenuItem('Open console.lua') then docs.open(puss._path .. '/core/console.lua') end
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Help') then
		active, show_imgui_demos = imgui.MenuItem('ImGUI Demos', nil, show_imgui_demos)
		active, show_tabs_demo = imgui.MenuItem('Tabs Demo', nil, show_tabs_demo)
		active, show_console_window = imgui.MenuItem('Conosle', nil, show_console_window)
		active, show_shutcut_window = imgui.MenuItem('Shutcut', nil, show_shutcut_window)
		imgui.Separator()
		if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
		imgui.Separator()
		if puss.debug then
			if imgui.MenuItem('Load Debugger') then
				if puss._sep=='\\' then
					os.execute(string.format('start /B %s\\%s %s\\tools\\debugger.lua', puss._path, puss._self, puss._path))
				else
					os.execute(string.format('%s/%s %s/tools/debugger.lua &', puss._path, puss._self, puss._path))
				end
			end
			if puss.debug('running') then
				if imgui.MenuItem('Stop debug') then
					puss.debug(false)
				end
			else
				if imgui.MenuItem('Start debug ...') then
					puss.debug(true)
				end
			end
		else
			if imgui.MenuItem('Reboot As Debug Mode') then
				puss._reboot_as_debug_level = 9
				main_ui:set_should_close(true)
			end
		end
		imgui.EndMenu()
	end
	imgui.EndMenuBar()

	if shotcuts.is_pressed('app/reload') then puss.reload() end
end

local function left_pane()
	imgui.BeginChild('PussLeftPane', 0, 0, false)
	main_ui:protect_pcall(filebrowser.update)
	imgui.EndChild()
end

local function pages_on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		docs.open(path)
	end
end

local function main_pane()
	imgui.BeginChild('PussPagesPane', 0, 0, false)
	if imgui.IsWindowHovered(ImGuiHoveredFlags_ChildWindows) then
		local files = imgui.GetDropFiles()
		if files then pages_on_drop_files(files) end
	end
	pages.update(main_ui)
	imgui.EndChild()
end

local MAIN_WINDOW_FLAGS = ( ImGuiWindowFlags_NoTitleBar
	| ImGuiWindowFlags_NoResize
	| ImGuiWindowFlags_NoMove
	| ImGuiWindowFlags_NoScrollbar
	| ImGuiWindowFlags_NoScrollWithMouse
	| ImGuiWindowFlags_NoCollapse
	| ImGuiWindowFlags_NoSavedSettings
	| ImGuiWindowFlags_MenuBar
	| ImGuiWindowFlags_NoBringToFrontOnFocus
	)

local function show_main_window(is_init)
	imgui.SetNextWindowPos(0, 0)
	imgui.SetNextWindowSize(imgui.GetIODisplaySize())
	imgui.Begin('PussMainWindow', nil, MAIN_WINDOW_FLAGS)
	main_menu()
	imgui.Columns(2)
	if is_init then imgui.SetColumnWidth(-1, 200) end
	left_pane()
	imgui.NextColumn()
	main_pane()
	imgui.NextColumn()
	imgui.Columns(1)
	imgui.End()
end

local function do_quit_update()
	if run_sign and main_ui:should_close() then
		if pages.save_all(false) then
			run_sign = false
		else
			imgui.OpenPopup('Quit?')
			main_ui:set_should_close(false)
		end
	end

	if imgui.BeginPopupModal('Quit?') then
		imgui.Text('Quit?')
		imgui.Separator()
		if imgui.Button('Save all & Quit') then
			if pages.save_all() then
				run_sign = false
			else
				imgui.CloseCurrentPopup()
				main_ui:set_should_close(false)
			end
		end
		imgui.SameLine()
		if imgui.Button('Quit without save') then
			run_sign = false
		end
		imgui.SameLine()
		if imgui.Button('Cancel') then
			imgui.CloseCurrentPopup()
			main_ui:set_should_close(false)
		end
		imgui.EndPopup()
	end
end

local function do_update(is_init)
	show_main_window(is_init)
	if show_imgui_demos then
		show_imgui_demos = imgui.ShowDemoWindow(show_imgui_demos)
	end
	if show_tabs_demo then
		show_tabs_demo = imgui.ShowTabsDemo('Tabs Demo', show_tabs_demo)
	end
	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
	if show_shutcut_window then
		show_shutcut_window = shotcuts.update(show_shutcut_window)
	end
	do_quit_update()
end

__exports.init = function()
	main_ui = imgui.Create('Puss - Editor', 1024, 768)
	_main_ui = main_ui
	main_ui:set_error_handle(puss.logerr_handle())
	main_ui(do_update, true)
end

__exports.uninit = function()
	main_ui:destroy()
	main_ui = nil
end

__exports.update = function()
	main_ui(do_update)
	return run_sign
end

