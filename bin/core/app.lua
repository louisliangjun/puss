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
show_console_window = show_console_window or false
show_shutcut_window = show_shutcut_window or false

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)

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
		active, show_console_window = imgui.MenuItem('Conosle', nil, show_console_window)
		active, show_shutcut_window = imgui.MenuItem('Shutcut', nil, show_shutcut_window)
		imgui.Separator()
		if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
		imgui.Separator()
		if puss.debug then
			if imgui.MenuItem('Load Debugger') then
				if puss._sep=='\\' then
					os.execute(string.format('start /B %s\\%s --main=core.debugger -X-console', puss._path, puss._self))
				else
					os.execute(string.format('%s/%s --main=core.debugger &', puss._path, puss._self))
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
	if is_init then
		local x, y = imgui.GetPlatformWindowRect()
		imgui.SetNextWindowPos(x, y)
	end
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

	if show_imgui_demos then
		show_imgui_demos = imgui.ShowDemoWindow(show_imgui_demos)
	end
	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
	if show_shutcut_window then
		show_shutcut_window = shotcuts.update(show_shutcut_window)
	end
end

local function do_quit_update()
	if run_sign and main_ui:should_close() then
		if pages.save_all(true) then
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

local function do_update()
	main_ui:protect_pcall(show_main_window)
	do_quit_update()
end

__exports.init = function()
	local title = 'Puss - Editor'
	puss._app_title = title
	main_ui = imgui.Create(title, 1024, 768)
	_main_ui = main_ui
	main_ui:set_error_handle(puss.logerr_handle())
	main_ui(show_main_window, true)
end

__exports.uninit = function()
	main_ui:destroy()
	main_ui = nil
end

__exports.update = function()
	main_ui(do_update)
	return run_sign
end

