-- core.app

local shotcuts = puss.import('core.shotcuts')
local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')
local samples = puss.import('core.samples')
local diskfs = puss.import('core.diskfs')
local miniline = puss.import('core.miniline')
local thread = puss.import('core.thread')
local search = puss.import('core.search')

local run_sign = true

local windows =
	{ {name='Console', module='core.console'}
	, {name='Shotcuts', module='core.shotcuts'}
	, {name='Search', module='core.search', visible=true}
	}

local helps =
	{ {name='ImGUI Demos', module=imgui.ShowDemoWindow}
	, {name='ImGUI Metrics', module=imgui.ShowMetricsWindow}
	, {name='Samples', module='core.samples'}
	}

if imgui.ShowFreetypeOptionsWindow then
	table.insert(helps, {name='Font Options', module=imgui.ShowFreetypeOptionsWindow})
end

local function menu_windows(wins)
	local active
	for i,v in ipairs(wins) do
		active, v.visible = imgui.MenuItem(v.name, nil, v.visible)
		if v.clicked and active then
			imgui.protect_pcall(v.clicked, v.visible, v.plat)
		end
	end
end

local function dummy_update(visible)
	return visible
end

local function load_window(w)
	local update = dummy_update
	local module_type = type(w.module)
	if module_type=='function' then
		update = w.module
	elseif module_type=='string' then
		local ok, m = puss.trace_pcall(puss.import, w.module)
		if ok then 
			update = m.update 
			if m.init then
				imgui.protect_pcall(m.init)
			end
		end
	end
	w.update = update
	return update
end

local function show_windows(wins)
	for i,v in ipairs(wins) do
		if v.visible then
			local ok, res = imgui.protect_pcall(v.update or load_window(v), true)
			if ok then
				v.visible = res
			end
		end
	end
end

local function file_skey_fetch(filepath)
	local ok, st = puss.stat(filepath, true)
	if not ok then return end
	return string.format('%s_%s_%s', st.size, st.mtime, st.ctime)
end

function docs_page_on_load(page_after_load, filepath)
	local ctx = diskfs.load(filepath)
	return page_after_load(ctx, file_skey_fetch(filepath))
end

function docs_page_on_save(page_after_save, filepath, ctx, file_skey)
	local now_file_skey = file_skey_fetch(filepath)
	if now_file_skey ~= file_skey then
		return page_after_save(false, now_file_skey)
	end
	local ok = diskfs.save(filepath, ctx)
	if not ok then
		return page_after_save(false, now_file_skey)
	end
	return page_after_save(ok, file_skey_fetch(filepath))
end

docs.setup(function(event, ...)
	local f = _ENV[event]
	if f then return f(...) end
end)

local function fs_list(dir, callback)
	callback(true, puss.file_list(dir, true))	-- list file & convert name to utf8
end

filebrowser.reset_select_callback(function(item, btn)
	if item.path then
		docs.reset_current_dir(item.path)
	elseif btn==ImGuiMouseButton_Left then
		docs.open(item.parent.path..'/'..item.name)
	else
		docs.reset_current_dir(item.parent.path)
	end
end)

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)

local function main_menu()
	local active, value
	if not imgui.BeginMenuBar() then return end
	if imgui.BeginMenu('File') then
		if shotcuts.menu_item('docs/new') then docs.new_page() end
		if shotcuts.menu_item('docs/save') then pages.save() end
		if imgui.MenuItem('Save as ...') then pages.save('rename') end
		if shotcuts.menu_item('docs/close') then pages.close() end
		imgui.Separator()
		if shotcuts.menu_item('page/close_all') then pages.close_all() end
		if shotcuts.menu_item('page/save_all') then pages.save_all() end
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Edit') then
		if shotcuts.menu_item('docs/find') then docs.edit_menu_click('docs/find') end
		if shotcuts.menu_item('docs/jump') then docs.edit_menu_click('docs/jump') end
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Setting') then
		local style_changed = imgui.ShowStyleSelector('Style')
		active, value = imgui.DragFloat('UI Scale', imgui.GetIO('FontGlobalScale'), 0.005, 0.5, 2.0, "%.1f")
		if active then imgui.SetIO('FontGlobalScale', value) end
		docs.setting(style_changed)
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Window') then
		if shotcuts.menu_item('miniline/open') then miniline.open() end
		imgui.Separator()
		menu_windows(windows)
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Help') then
		menu_windows(helps)
		imgui.Separator()
		if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
		imgui.Separator()
		if puss.debug then
			if imgui.MenuItem('Load Debugger') then
				if puss.OS=='win32' then
					os.execute(string.format('start %s\\%s --main=core.debugger -X-console', puss._path, puss._self))
				else
					os.execute(string.format('%s/%s --main=core.debugger &', puss._path, puss._self))
				end
			end
			if puss.debug() then
				if imgui.MenuItem('Stop debug') then
					puss.debug(false)
				end
			else
				if imgui.MenuItem('Start debug ...') then
					puss.debug(true, nil, puss._app_title)
				end
			end
		else
			if imgui.MenuItem('Reboot As Debug Mode') then
				puss._reboot_as_debug_level = 9
				imgui.set_should_close(true)
			end
		end
		imgui.EndMenu()
	end
	imgui.EndMenuBar()

	if shotcuts.is_pressed('app/reload') then puss.reload() end
end

local EDITOR_WINDOW_FLAGS = ( ImGuiWindowFlags_NoScrollbar
	| ImGuiWindowFlags_NoScrollWithMouse
	)

local function editor_window()
	imgui.Begin("Editor", nil, EDITOR_WINDOW_FLAGS)
	local drop_files_here = imgui.GetDropFiles(true)
	if drop_files_here then
		for path in drop_files_here:gmatch('(.-)\n') do
			docs.open(path)
		end
	end
	pages.update()
	miniline.update()
	imgui.End()
end

local function filebrowser_window()
	imgui.Begin("FileBrowser")
	filebrowser.update(fs_list)
	imgui.End()
end

local MAIN_DOCK_WINDOW_FLAGS = ( ImGuiWindowFlags_NoTitleBar
	| ImGuiWindowFlags_NoCollapse
	| ImGuiWindowFlags_NoResize
	| ImGuiWindowFlags_NoMove
	| ImGuiWindowFlags_NoScrollbar
	| ImGuiWindowFlags_NoScrollWithMouse
	| ImGuiWindowFlags_NoSavedSettings
	| ImGuiWindowFlags_NoBringToFrontOnFocus
	| ImGuiWindowFlags_NoNavFocus
	| ImGuiWindowFlags_NoDocking
	| ImGuiWindowFlags_MenuBar
	)

local function show_main_window()
	local vid, x, y, w, h = imgui.GetMainViewport()
	imgui.SetNextWindowPos(x, y)
	imgui.SetNextWindowSize(w, h)
	imgui.SetNextWindowViewport(vid)

	imgui.PushStyleVar(ImGuiStyleVar_WindowRounding, 0)
	imgui.PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0)
	imgui.PushStyleVar(ImGuiStyleVar_WindowPadding, 0, 0)
	imgui.Begin('PussMainDockWindow', nil, MAIN_DOCK_WINDOW_FLAGS)
	imgui.PopStyleVar(3)
	imgui.DockSpace(imgui.GetID('#MainDockspace'), 0, 0, ImGuiDockNodeFlags_AutoHideTabBar)
	main_menu()
	imgui.End()

	local menu_size = 24
	local left_size = 260

	imgui.SetNextWindowPos(x, y + menu_size, ImGuiCond_FirstUseEver)
	imgui.SetNextWindowSize(left_size, h - menu_size, ImGuiCond_FirstUseEver)
	filebrowser_window()

	imgui.SetNextWindowPos(x + left_size, y + menu_size, ImGuiCond_FirstUseEver)
	imgui.SetNextWindowSize(w - left_size, h - menu_size, ImGuiCond_FirstUseEver)
	editor_window()

	show_windows(windows)
	show_windows(helps)
end

local function do_quit_update()
	if run_sign and imgui.should_close() then
		if pages.save_all(true) then
			run_sign = false
		else
			imgui.OpenPopup('Quit?')
			imgui.set_should_close(false)
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
				imgui.set_should_close(false)
			end
		end
		imgui.SameLine()
		if imgui.Button('Quit without save') then
			run_sign = false
		end
		imgui.SameLine()
		if imgui.Button('Cancel') then
			imgui.CloseCurrentPopup()
			imgui.set_should_close(false)
		end
		imgui.EndPopup()
	end
end

local function do_update()
	puss.async_service_update()
	thread.update()
	imgui.protect_pcall(show_main_window)
	do_quit_update()
end

__exports.init = function()
	local title = 'Puss - Editor'
	puss._app_title = title
	run_sign = true
	imgui.create(title, 1024, 768, 'puss_editor.ini', function()
		local font_path = string.format('%s/fonts', puss._path)
		local files = puss.file_list(font_path)
		for _, name in ipairs(files) do
			if name:match('^.+%.[tT][tT][fF]$') then
				local lang = name:match('^.-%.(%w+)%.%w+$')
				imgui.AddFontFromFileTTF(string.format('%s/%s', font_path, name), 13, lang)
			end
		end
	end)
	imgui.update(show_main_window)

	filebrowser.append_folder(puss.filename_format(puss._path .. '/core', true), fs_list)

	if puss.debug and (not puss.debug()) then puss.debug(true, nil, title) end
end

__exports.uninit = function()
	imgui.destroy()
end

__exports.update = function()
	imgui.update(do_update)
	return run_sign
end
