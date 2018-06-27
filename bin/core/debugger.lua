-- debugger.lua

local diskfs = puss.import('core.diskfs')
local shotcuts = puss.import('core.shotcuts')
local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local demos = puss.import('core.demos')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')
local net = puss.import('core.net')

filebrowser.setup(diskfs)

docs.setup(diskfs, function(page, event, ...)
	local f = _ENV[event]
	if f then return f(page, ...) end
end)

local run_sign = true
local main_ui = _main_ui

local socket = _socket

local stack_list = {}
local stack_current = 1

show_console_window = show_console_window or false

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)

shotcuts.register('debugger/bp', 'set/unset bp', 'F9', false, false, false, false)

shotcuts.register('debugger/continue', 'continue', 'F5', false, false, false, false)
shotcuts.register('debugger/step_over', 'step over', 'F10', false, false, false, false)
shotcuts.register('debugger/step_into', 'step into', 'F11', false, false, false, false)
shotcuts.register('debugger/step_out', 'step out', 'F11', false, true, false, false)

local stubs = {}

stubs.breaked = function()
	net.send(socket, 'fetch_stack')
end

stubs.fetch_stack = function(ok, res)
	stack_list = {} 
	if not ok then return print('fetch_stack failed:', ok, res) end
	if type(res)~='table' then return print('fetch_stack failed:', ok, res) end
	stack_list = res
	stack_current = 1
	local info = stack_list[stack_current]
	if info then
		net.send(socket, 'fetch_vars', info.level)
		local fname = info.source:match('^@(.+)$')
		if fname then docs.open(fname, info.currentline-1) end
	end
end

stubs.fetch_vars = function(level, ok, locals, ups, varargs)
	if not ok then return print('fetch_vars failed:', locals, ups, varargs) end
	for i,info in ipairs(stack_list) do
		if info.level==level then
			print('vars', level, #locals, #ups, #varargs)
			info.locals, info.ups, info.varargs = locals, ups, varargs
			break
		end
	end
end

stubs.set_bp = function(fname, line, bp)
	fname = fname:match('^@(.-)%s*$') or fname
	docs.margin_set(fname, line, bp)
end

local function dispatch(cmd, ...)
	print('debugger recv:', cmd, ...)
	local h = stubs[cmd]
	if not h then return print('unknown stub:', cmd) end
	h(...)
end

local function shortcuts_update()
	if shotcuts.is_pressed('debugger/step_over') then net.send(socket, 'step_over') end
	if shotcuts.is_pressed('debugger/step_into') then net.send(socket, 'step_into') end
	if shotcuts.is_pressed('debugger/step_out') then net.send(socket, 'step_out') end
	if shotcuts.is_pressed('debugger/continue') then net.send(socket, 'continue') end
end

local function debug_toolbar()
	if socket and socket:valid() then
		if imgui.Button("disconnect") then
			socket:close()
		end
	else
		if imgui.Button("connect") then
			_socket = net.connect('127.0.0.1', 9999)
			socket = _socket
		end
	end
	imgui.SameLine()
	if imgui.Button("step") then
		net.send(socket, 'step_into')
	end
	imgui.SameLine()
	if imgui.Button("continue") then
		net.send(socket, 'continue')
	end
end

local function draw_stack()
	local clicked = nil
	for i,info in ipairs(stack_list) do
		local label = string.format('%s:%d', info.short_src, info.currentline)
		imgui.TreeNodeEx(tostring(info), ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
		if imgui.IsItemClicked() then
			stack_current = i
			clicked = info
		end
	end
	if clicked then
		net.send(socket, 'fetch_vars', clicked.level)
		local fname = clicked.source:match('^@(.+)$')
		if fname then docs.open(fname, clicked.currentline-1) end
	end
end

local function draw_vars()
	local info = stack_list[stack_current]
	if not info then
		return imgui.Text('<empty>')
	end
	if info.locals then
		for n,v in pairs(info.locals) do
			local label = string.format('VAR %s: %s', n, v)
			imgui.TreeNodeEx(label, ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
			if imgui.IsItemClicked() then print(label) end
		end
	end
	if info.ups then
		for n,v in pairs(info.ups) do
			local label = string.format('UPV %s: %s', n, v)
			imgui.TreeNodeEx(label, ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
			if imgui.IsItemClicked() then print(label) end
		end
	end
	if info.varargs then
		for n,v in pairs(info.varargs) do
			local label = string.format('... %s: %s', n, v)
			imgui.TreeNodeEx(label, ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
			if imgui.IsItemClicked() then print(label) end
		end
	end
end

local function debug_pane(main_ui)
	net.update(socket, dispatch)

	shortcuts_update()

	main_ui:protect_pcall(debug_toolbar)
	if imgui.CollapsingHeader('Stack', ImGuiTreeNodeFlags_DefaultOpen) then
		main_ui:protect_pcall(draw_stack)
	end
	if imgui.CollapsingHeader('Vars', ImGuiTreeNodeFlags_DefaultOpen) then
		main_ui:protect_pcall(draw_vars)
	end
end

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
	imgui.BeginChild('PussLeftPane', 0, 0, false)
	main_ui:protect_pcall(filebrowser.update)
	imgui.EndChild()
end

local function pages_on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		docs.open(path)
	end
end

local function trigger_bp(page, line)
	local sv = page.sv
	local bp = (sv:MarkerGet(line) & 0x01)==0
	local fname = '@'..puss.filename_format(page.filepath)
	net.send(socket, 'set_bp', fname, line+1, bp)
end

function docs_page_on_draw(page)
	if shotcuts.is_pressed('debugger/bp') then
		local line = page.sv:LineFromPosition(page.sv:GetCurrentPos())
		trigger_bp(page, line)
	end
end

function docs_page_on_create(page)
	print('page create')
end

function docs_page_on_margin_click(page, modifiers, pos, margin)
	print(page, modifiers, pos, margin)
	-- print(page.sv, sv:MarkerGet(line))
	local line = page.sv:LineFromPosition(pos)
	trigger_bp(page, line)
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

local function right_pane()
	imgui.BeginChild('PussRightPane', 0, 0, false)
	main_ui:protect_pcall(debug_pane, main_ui)
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
	imgui.Columns(3)
	if is_init then imgui.SetColumnWidth(-1, 200) end
	left_pane()
	imgui.NextColumn()
	if is_init then imgui.SetColumnWidth(-1, imgui.GetWindowWidth()-400) end
	main_pane()
	imgui.NextColumn()
	right_pane()
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

