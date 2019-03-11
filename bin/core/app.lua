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

show_imgui_demos = show_imgui_demos or false
show_imgui_metrics = show_imgui_metrics or false

show_samples_window = show_samples_window or false
show_console_window = show_console_window or false
show_shutcut_window = show_shutcut_window or false
show_search_window = show_search_window or false

function docs_page_on_load(page_after_load, filepath)
	local ctx = diskfs.load(filepath)
	return page_after_load(ctx)
end

function docs_page_on_save(page_after_save, filepath, ctx)
	local ok = diskfs.save(filepath, ctx)
	page_after_save(ok)
end

docs.setup(function(event, ...)
	local f = _ENV[event]
	if f then return f(...) end
end)

local function fs_list(dir, callback)
	callback(true, puss.file_list(dir, true))	-- list file & convert name to utf8
end

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
	if imgui.BeginMenu('Help') then
		active, show_imgui_demos = imgui.MenuItem('ImGUI Demos', nil, show_imgui_demos)
		active, show_imgui_metrics = imgui.MenuItem('ImGUI Metrics', nil, show_imgui_metrics)
		active, show_samples_window = imgui.MenuItem('Samples', nil, show_samples_window)
		active, show_console_window = imgui.MenuItem('Conosle', nil, show_console_window)
		active, show_shutcut_window = imgui.MenuItem('Shutcut', nil, show_shutcut_window)
		active, show_search_window = imgui.MenuItem('Search', nil, show_search_window)
		imgui.Separator()
		if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
		imgui.Separator()
		if puss.debug then
			if imgui.MenuItem('Load Debugger') then
				if os.getenv('OS')=='Windows_NT' then
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
	filebrowser.update()
	local drop_folders_here = imgui.GetDropFiles(true)
	if drop_folders_here then
		for path in drop_folders_here:gmatch('(.-)\n') do
			local is_folder = true
			local local_path = puss.utf8_to_local(path)
			local f = io.open(local_path, 'r')
			if f then
				local ctx = f:read(64)
				is_folder = (ctx==nil)
				f:close()
			end
			if is_folder then
				filebrowser.append_folder(puss.filename_format(path, true), fs_list)
			end
		end
	end
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
	imgui.DockSpace(imgui.GetID('#MainDockspace'))
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

	if show_imgui_demos then
		show_imgui_demos = imgui.ShowDemoWindow(show_imgui_demos)
	end
	if show_imgui_metrics then
		show_imgui_metrics = imgui.ShowMetricsWindow(show_imgui_metrics)
	end
	if show_samples_window then
		show_samples_window = samples.update(show_samples_window)
	end
	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
	if show_shutcut_window then
		show_shutcut_window = shotcuts.update(show_shutcut_window)
	end
	if show_search_window then
		show_search_window = search.update(show_search_window)
	end
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
				imgui.AddFontFromFileTTF(string.format('%s/%s', font_path, name), 14, lang)
			end
		end
	end)
	imgui.update(show_main_window)

	filebrowser.append_folder(puss.filename_format(puss._path .. '/core', true), fs_list)
end

__exports.uninit = function()
	imgui.destroy()
end

__exports.update = function()
	imgui.update(do_update)
	return run_sign
end
