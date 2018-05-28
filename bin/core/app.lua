-- app.lua

local console = puss.import('core.console')
local docs = puss.import('core.docs')
local demos = puss.import('core.demos')

main_ui = main_ui	-- reload

show_imgui_demos = show_imgui_demos or false
show_tabs_demo = show_tabs_demo or false
show_console_window = show_console_window or false

_pages = _pages or {}
_index = _index or setmetatable({}, {__mode='v'})
local pages = _pages
local index = _index
local next_active_page_label = nil

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
    if imgui.BeginMenu('File') then
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
    	imgui.Separator()
    	if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
        imgui.EndMenu()
    end
    imgui.EndMenuBar()

	if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_F12, true) then puss.reload() end
end

local function tabs_bar()
    imgui.BeginTabBar("PussMainTabsBar", ImGuiTabBarFlags_SizingPolicyFit)

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
	imgui.Begin("PussMainWindow", nil, flags)
	main_menu(ui)
	tabs_bar(ui)
	imgui.End()
end

local function do_update()
	show_main_window(ui)
	if show_imgui_demos then
		show_imgui_demos = imgui.ShowDemoWindow(show_imgui_demos)
	end
	if show_tabs_demo then
		show_tabs_demo = imgui.ShowTabsDemo('Tabs Demo', show_tabs_demo)
	end
	if show_console_window then
		show_console_window = console.update(show_console_window)
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

__exports.lookup_page = function(id)
	return index[id]
end

__exports.init = function()
	main_ui = imgui.Create("Puss - Editor", 1024, 768)
	main_ui:set_error_handle(puss.logerr_handle())
end

__exports.uninit = function()
	main_ui:destroy()
	main_ui = nil
end

__exports.update = function()
	imgui.WaitEventsTimeout()

	return main_ui(puss.trace_pcall, do_update, main_ui)
end

