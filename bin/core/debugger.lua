-- debugger.lua

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
local sock = _sock

show_console_window = show_console_window or false

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
	if imgui.BeginMenu('File') then
		if imgui.MenuItem('Add puss path FileBrowser') then filebrowser.append_folder(puss.local_to_utf8(puss._path)) end
		imgui.EndMenu()
	end
	if imgui.BeginMenu('Help') then
		active, show_console_window = imgui.MenuItem('Conosle', nil, show_console_window)
		imgui.Separator()
		if imgui.MenuItem('Reload', 'Ctrl+F12') then puss.reload() end
		imgui.EndMenu()
	end
	imgui.EndMenuBar()

	if shotcuts.is_pressed('app/reload') then puss.reload() end
end

local function left_pane()
	main_ui:protect_pcall(filebrowser.update)
end

local function pages_on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		docs.open(path)
	end
end

-- debugger client
-- 
local function disconnect()
	if sock then
		local s = sock
		sock = nil
		_sock = nil
		s:close()
	end
end

local function debug_call(cmd, ...)
	if not sock then
		print('need connect')
		return
	end
	sock:send(puss.pickle_pack(cmd, ...))
	local res, msg = sock:recv()
	if res < 0 then
		print('debugger recv error:', res, msg)
	elseif res==0 then
		print('debugger disconnected')
		disconnect()
	else
		print('debugger recv:', puss.pickle_unpack(msg))
	end
end

local function debug_pane()
	if sock then
		if imgui.Button("disconnect") then
			disconnect()
		end
	else
		if imgui.Button("connect") then
			disconnect()
			_sock = puss_socket.socket_create()
			sock = _sock
			print(sock:connect('127.0.0.1', 9999))
		end
	end
	imgui.SameLine()
	if imgui.Button("step_into") then
		debug_call('step_into')
	end
	imgui.SameLine()
	if imgui.Button("continue") then
		debug_call('continue')
	end
	imgui.SameLine()
	if imgui.Button("dostring") then
		debug_call('host_pcall', 'puss.trace_dostring', [[
			for lv=6,1000 do
				local info = debug.getinfo(lv, 'Slut')
				if not info then break end
				print('	LV:', lv)
				for k,v in pairs(info) do print('	', k,v) end
			end
			print(debug.getlocal(6, 1))
			print(debug.getlocal(9, -1))
			do
				local info = debug.getinfo(9, 'f')
				print(debug.getupvalue(info.func, 2))
			end
		]])
	end
	imgui.SameLine()
	if imgui.Button("stack") then
		debug_call('host_pcall', 'puss._debug.fetch_stack')
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
	debug_pane()
	main_pane()
	imgui.Columns(1)
	imgui.End()
end

local function do_update(is_init)
	show_main_window(is_init)
	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
	if run_sign and main_ui:should_close() then
		run_sign = false
	end
end

__exports.init = function()
	main_ui = imgui.Create('Puss - Debugger', 1024, 768)
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

