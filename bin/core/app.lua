-- app.lua

local shotcuts = puss.import('core.shotcuts')
local docs = puss.import('core.docs')
local demos = puss.import('core.demos')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')

local run_sign = true

main_ui = main_ui	-- reload

show_imgui_demos = show_imgui_demos or false
show_tabs_demo = show_tabs_demo or false
show_console_window = show_console_window or false
show_shutcut_window = show_shutcut_window or false

_pages = _pages or {}
_index = _index or setmetatable({}, {__mode='v'})
local pages = _pages
local index = _index
local next_active_page_label = nil

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
			if imgui.MenuItem('Start debug & wait connect...') then
				puss.debug()
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

	if shotcuts.is_pressed('app', 'reload') then puss.reload() end
end

local function left_pane()
	main_ui:protect_pcall(filebrowser.update)
end

local function pages_on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		docs.open(path)
	end
end

local function tabs_bar()
    imgui.BeginTabBar('PussMainTabsBar', ImGuiTabBarFlags_SizingPolicyFit)

	-- set active
	if next_active_page_label then
		local label = next_active_page_label
		next_active_page_label = nil
		local page = index[label]
		if page then imgui.SetTabItemSelected(label) end
	end

	-- draw tabs
	for i, page in ipairs(pages) do
		local selected
		page.was_open = page.open
		selected, page.open = imgui.TabItem(page.label, page.open, page.unsaved and ImGuiTabItemFlags_UnsavedDocument or ImGuiTabItemFlags_None)
		if selected then
			local draw = page.module.tabs_page_draw
			if draw then main_ui:protect_pcall(draw, page) end
		end
	end
	imgui.EndTabBar()

	-- close 
	for i=#pages,1,-1 do
		local page = pages[i]
		if not page.open then
			local close = page.module.tabs_page_close
			if close then main_ui:protect_pcall(close, page) end
		end
		if not page.was_open then
			local destroy = page.module.tabs_page_destroy
			if destroy then main_ui:protect_pcall(destroy, page) end
			index[page.label] = nil
			table.remove(pages, i)
		end
	end
end

local function pages_save_all()
	local all_saved = true
	for _,page in ipairs(pages) do
		local save = page.module.tabs_page_save
		if save then save(page) end
		if page.unsaved then all_saved = false end
	end
	print(all_saved)
	return all_saved
end

local function show_main_window()
	local flags = ( ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		)
	imgui.SetNextWindowPos(0, 0)
	imgui.SetNextWindowSize(imgui.GetIODisplaySize())
	imgui.Begin('PussMainWindow', nil, flags)
	main_menu()
	imgui.BeginChild('PussLeftPane', 200, 0, true)
		left_pane()
	imgui.EndChild()
	imgui.SameLine()
	imgui.BeginChild('PussPagesPane', 0, 0, true)
		if imgui.IsWindowHovered(ImGuiHoveredFlags_ChildWindows) then
			local files = imgui.GetDropFiles()
			if files then pages_on_drop_files(files) end
		end
		tabs_bar()
	imgui.EndChild()
	imgui.End()
end

local function do_update()
	show_main_window()
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

	if run_sign and main_ui:should_close() then
		local all_saved = true
		for _, page in ipairs(pages) do
			if page.unsaved then
				all_saved = false
				break
			end
		end
		if all_saved then
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
			if pages_save_all() then
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

__exports.create_page = function(label, module)
	local page = index[label]
	if page then return page end
	local page = {label=label, module=module, was_open=true, open=true}
	table.insert(pages, page)
	index[label] = page
	return page
end

__exports.active_page = function(label)
	next_active_page_label = label
end

__exports.lookup_page = function(label)
	return index[label]
end

__exports.init = function()
	main_ui = imgui.Create('Puss - Editor', 1024, 768)
	main_ui:set_error_handle(puss.logerr_handle())
end

__exports.uninit = function()
	main_ui:destroy()
	main_ui = nil
end

__exports.update = function()
	main_ui(do_update)
	return run_sign
end

