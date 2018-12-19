-- debugger.lua

local diskfs = puss.import('core.diskfs')
local shotcuts = puss.import('core.shotcuts')
local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')
local net = puss.import('core.net')

filebrowser.setup(diskfs)

docs.setup(diskfs, function(page, event, ...)
	local f = _ENV[event]
	if f then return f(page, ...) end
end)

local LEAF_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
local FOLD_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick

local run_sign = true

local BROADCAST_PORT = 9999
local broadcast_recv = net.create_udp_broadcast_recver(BROADCAST_PORT)

local hosts = nil

local socket = _socket

local dummy_vars = {}

stack_list = stack_list or {}
stack_vars = stack_vars or {}
stack_current = stack_current or 1

show_console_window = show_console_window or false

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)

shotcuts.register('debugger/bp', 'set/unset bp', 'F9', false, false, false, false)

shotcuts.register('debugger/continue', 'continue', 'F5', false, false, false, false)
shotcuts.register('debugger/step_over', 'step over', 'F10', false, false, false, false)
shotcuts.register('debugger/step_into', 'step into', 'F11', false, false, false, false)
shotcuts.register('debugger/step_out', 'step out', 'F11', false, true, false, false)

local function stack_list_clear()
	stack_list = {}
	stack_current = 1
end

local function stack_vars_replace(var)
	local old = stack_vars[var[1]]
	if old then
		for i,v in ipairs(var) do old[i]=v end
		var = old
	else
		stack_vars[var[1]] = var
	end
	return var
end

local function vars_sort_by_name(a, b)
	local an, bn = a[4], b[4]
	if an==bn then return a[1] < b[1] end
	return an<bn
end

local function locate_to_file(fname, line)
	if docs.open(fname, line) then return end

	fname = puss._path .. '/' .. fname
	if docs.open(fname, line) then return end
end

local stubs = {}

stubs.continued = function()
	stack_list_clear()
end

stubs.breaked = function(ok, res)
	stack_vars = {}
	if not ok then return print('fetch_stack failed:', ok, res) end
	if type(res)~='table' then return print('fetch_stack failed:', ok, res) end
	stack_list = res
	stack_current = 1
	local info = stack_list[stack_current]
	if info then
		net.send(socket, 'fetch_vars', info.level)
		local fname = info.source:match('^@(.+)$')
		if fname then locate_to_file(fname, info.currentline-1) end
	end
end

stubs.fetch_vars = function(level, ok, vars)
	if not ok then return print('fetch_vars failed:', vars) end
	local info
	for i,v in ipairs(stack_list) do
		if v.level==level then
			info = v
			break
		end
	end
	if info then
		-- print('vars', level, #vars)
		info.vars = vars
		for i=1,#vars do vars[i] = stack_vars_replace(vars[i]) end
		table.sort(vars, vars_sort_by_name)
	end
end

stubs.fetch_subs = function(key, ok, subs)
	if not ok then return print('fetch_subs failed:', subs) end
	local var = stack_vars[key]
	if var then
		var.subs = (subs and next(subs)) and subs or false
		-- print('fetch subs:', key, var.subs)
		if subs then
			for i=1,#subs do subs[i] = stack_vars_replace(subs[i]) end
			table.sort(subs, vars_sort_by_name)
		end
	end
end

stubs.set_bp = function(fname, line, bp, err)
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

local function recver_update()
	local data, addr = broadcast_recv()
	if not data then return end
	if not hosts then return end
	--print(data, addr, hosts)

	-- local machine, use addr
	local ip, port, title = data:match('^%[PussDebug%]|(%d+%.%d+%.%d+%.%d+):(%d+)|?(.*)$')
	if ip=='0.0.0.0' then ip = addr:match('^(%d+%.%d+%.%d+%.%d+):%d+') end
	if ip then
		local key = ip .. ':' .. port
		local info = hosts[key]
		if info then
			info.timestamp = os.time()
		else
			info = {label=string.format('%s:%s (%s)', ip, port, title), ip=ip, port=tonumber(port), title=title, timestamp=os.time()}
			table.insert(hosts, info)
			hosts[key] = info
		end
	end
	return true
end

local function tool_button(label, hint)
	local clicked = imgui.Button(label)
	if imgui.IsItemHovered() then
		imgui.BeginTooltip()
		imgui.Text(hint)
		imgui.EndTooltip()
	end
	return clicked
end

local function debug_toolbar()
	if socket and socket:valid() then
		if tool_button('断开', '断开连接') then
			socket:close()
			socket = nil
			stack_list_clear()
		end
	else
		if tool_button('连接', '连接...') then
			hosts = {}
			imgui.OpenPopup('Connect ...')
			imgui.SetNextWindowSize(430, 320)
		end
	end
	imgui.SameLine()
	if tool_button('>>', '继续运行 (F5)') then
		net.send(socket, 'continue')
	end
	imgui.SameLine()
	if tool_button('|>', '单步 (F10)') then
		net.send(socket, 'step_over')
	end
	imgui.SameLine()
	if tool_button('=>', '步入 (F11)') then
		net.send(socket, 'step_into')
	end
	imgui.SameLine()
	if tool_button('<=', '跳出 (Shift+F11)') then
		net.send(socket, 'step_out')
	end
	imgui.SameLine()
	if tool_button('Err', '异常断点') then
		net.send(socket, 'capture_error')
	end
	imgui.SameLine()
	if tool_button('ASM', '显示反汇编') then
		net.send(socket, 'fetch_disasm', stack_current)
	end

	if imgui.BeginPopupModal('Connect ...') then
		if imgui.Button("Quit") then
			imgui.CloseCurrentPopup()
		else
			imgui.SameLine()
			imgui.Text('or wait hosts & click to connect ...')
			imgui.Separator()
			for _, info in ipairs(hosts) do
				imgui.TreeNodeEx(v, LEAF_FLAGS, info.label)
				if imgui.IsItemClicked() then
					imgui.CloseCurrentPopup()
					hosts = nil
					_socket = net.connect(info.ip, info.port)
					if socket then socket:close() end
					socket = _socket
				end
			end
		end
		imgui.EndPopup()
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
		if not clicked.vars then
			clicked.vars = dummy_vars
			net.send(socket, 'fetch_vars', clicked.level)
		end
		local fname = clicked.source:match('^@(.+)$')
		if fname then locate_to_file(fname, clicked.currentline-1) end
	end
end

local has_sub_types =
	{ ['T'] = true
	, ['U'] = true
	}

local has_modify_types =
	{ ['-'] = true
	, ['B'] = true
	, ['N'] = true
	, ['S'] = true
	}

local function draw_subs(stack_current, subs)
	for _,v in ipairs(subs) do
		local vt = v[3]
		local has_subs = has_sub_types[vt] and v.subs~=false
		imgui.PushStyleColor(ImGuiCol_Text, 0.75, 0.75, 1, 1)
		local show = imgui.TreeNodeEx(v, has_subs and FOLD_FLAGS or LEAF_FLAGS, v[4])
		imgui.PopStyleColor()
		if has_subs and imgui.IsItemClicked() then
			if v.subs==nil then
				v.subs = dummy_vars
				net.send(socket, 'fetch_subs', v[1])
			end
		end
		imgui.SameLine()
		imgui.TextColored(1, 1, 0.75, 1, tostring(v[5]))
		-- if has_modify_types[vt] then
		-- 	imgui.SameLine()
		-- 	imgui.SmallButton('modify')
		-- end
		if has_subs and show then
			if v.subs then draw_subs(stack_current, v.subs) end
			imgui.TreePop()
		end
	end
end

local function draw_vars()
	local info = stack_list[stack_current]
	if not info then return end
	if info.vars then
		draw_subs(stack_current, info.vars)
	end
end

local function debug_window()
	imgui.Begin('DebugWindow')
	net.update(socket, dispatch)

	shortcuts_update()

	imgui.protect_pcall(debug_toolbar)
	if imgui.CollapsingHeader('Stack', ImGuiTreeNodeFlags_DefaultOpen) then
		imgui.protect_pcall(draw_stack)
	end
	if imgui.CollapsingHeader('Vars', ImGuiTreeNodeFlags_DefaultOpen) then
		imgui.protect_pcall(draw_vars)
	end
	imgui.End()
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

local function trigger_bp(page, line)
	local fname = '@'..puss.filename_format(page.filepath)
	net.send(socket, 'set_bp', fname, line+1)
end

function docs_page_before_draw(page)
	if shotcuts.is_pressed('debugger/bp') then
		local line = page.sv:LineFromPosition(page.sv:GetCurrentPos())
		trigger_bp(page, line)
	end
end

function docs_page_after_draw(page)
	local info = stack_list[stack_current]
	if not info then return end
	local fname = info.source:match('^@(.+)$')
	if not fname then return end
	local line = info.currentline
	local sv = page.sv
	local x,y = imgui.GetWindowPos()
	local w,h = imgui.GetWindowSize()
	local top = sv:GetFirstVisibleLine()
	local th = sv:TextHeight(1)
	imgui.WindowDrawListAddRectFilled(x,y+(line-top-1)*th,x+w,y+(line-top)*th,0x3FFF00FF)
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

local function pages_on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		docs.open(path)
	end
end

local EDITOR_WINDOW_FLAGS = ( ImGuiWindowFlags_NoScrollbar
	| ImGuiWindowFlags_NoScrollWithMouse
	)

local function editor_window()
	imgui.Begin("Editor", nil, EDITOR_WINDOW_FLAGS)
	if imgui.IsWindowHovered(ImGuiHoveredFlags_ChildWindows) then
		local files = imgui.GetDropFiles()
		if files then pages_on_drop_files(files) end
	end
	pages.update()
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
	filebrowser.update()

	imgui.SetNextWindowPos(x + left_size, y + menu_size, ImGuiCond_FirstUseEver)
	imgui.SetNextWindowSize(w - left_size, h - menu_size, ImGuiCond_FirstUseEver)
	editor_window()

	debug_window()

	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
end

local function do_update()
	imgui.protect_pcall(show_main_window)

	for i=1,64 do
		if not recver_update() then break end
	end

	if run_sign and imgui.should_close() then
		run_sign = false
	end
end

__exports.init = function()
	imgui.create('Puss - Debugger', 1024, 768, 'puss_debugger.ini', function()
		local font_path = string.format('%s%sfonts', puss._path, puss._sep)
		local files = puss.file_list(font_path)
		for _, name in ipairs(files) do
			if name:match('^.+%.[tT][tT][fF]$') then
				local lang = name:match('^.-%.(%w+)%.%w+$')
				imgui.AddFontFromFileTTF(string.format('%s%s%s', font_path, puss._sep, name), 14, lang)
			end
		end
	end)
	imgui.update(show_main_window, true)
end

__exports.uninit = function()
	imgui.destroy()
end

__exports.update = function()
	imgui.update(do_update)
	return run_sign
end

