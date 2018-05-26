-- app.lua

local docs = puss.import('core.docs')
local console = puss.import('core.console')

main_ui = main_ui	-- reload

show_imgui_demos = show_imgui_demos or false
show_tabs_demo = show_tabs_demo or false
show_console_window = show_console_window or false

_pages = _pages or {}
_index = _index or setmetatable({}, {__mode='v'})
local pages = _pages
local index = _index

local function create_page(label, module)
	local page = index[label]
	if page then return page end 
	local page = {label=label, module=module, show=true}
	table.insert(pages, page)
	index[label] = page
	return page
end

function tabs_page_draw(page)
	imgui.Text(page.label)
	imgui.Text(string.format('DemoPage: %s %s', page, page.draw))
end

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
    if imgui.BeginMenu('File') then
    	if imgui.MenuItem('Test Add Tab') then create_page('Page ' .. (#pages + 1), _ENV) end
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
	for i, page in ipairs(pages) do
	    local active
		active, page.show = imgui.TabItem(page.label, page.show, page.unsaved and ImGuiTabItemFlags_UnsavedDocument or ImGuiTabItemFlags_None)
		if active then
			local draw = page.module.tabs_page_draw
			if draw then
				local st,sb = main_ui:stack_protect_begin()
				puss.trace_pcall(draw, page)
				main_ui:stack_protect_end(st,sb)
			end
		end
	end
	imgui.EndTabBar()
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

local function source_editor_main()
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

__exports.create_page = create_page

__exports.lookup_page = function(id)
	return index[id]
end

__exports.init = function()
	main_ui = imgui.Create("Puss - Editor", 1024, 768)
end

__exports.uninit = function()
	main_ui:destroy()
	main_ui = nil
end

__exports.update = function()
	return main_ui(puss.trace_pcall, source_editor_main, main_ui)
end

