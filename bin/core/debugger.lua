-- core.debugger

local shotcuts = puss.import('core.shotcuts')
local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local filebrowser = puss.import('core.filebrowser')
local console = puss.import('core.console')
local miniline = puss.import('core.miniline')
local net = puss.import('core.net')
local thread = puss.import('core.thread')

docs.setup(function(event, ...)
	local f = _ENV[event]
	if f then return f(...) end
end, true)

local LEAF_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
local FOLD_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick

local run_sign = true
local socket = _socket
local GROUP_KEY = 'socket'	-- NOTICE : attach to socket if need multi-socket

local BROADCAST_PORT = 7654
local broadcast_recv = net.create_udp_broadcast_recver(BROADCAST_PORT)

local dummy_vars = {}

stack_list = stack_list or {}
stack_vars = stack_vars or {}
stack_current = stack_current or 1

show_console_window = show_console_window or false
use_local_fs = use_local_fs or (use_local_fs==nil)

shotcuts.register('app/reload', 'Reload scripts', 'F12', true, false, false, false)
shotcuts.register('debugger/bp', 'set/unset bp', 'F9', false, false, false, false)
shotcuts.register('debugger/continue', 'continue', 'F5', false, false, false, false)
shotcuts.register('debugger/step_over', 'step over', 'F10', false, false, false, false)
shotcuts.register('debugger/step_into', 'step into', 'F11', false, false, false, false)
shotcuts.register('debugger/step_out', 'step out', 'F11', false, true, false, false)

local function stack_list_clear()
	stack_list, stack_vars, stack_current = {}, {}, 1
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

local function stack_vars_merge(level, vars)
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

local function stack_vars_fill(key, subs)
	local var = stack_vars[key]
	if not var then return end
	var.subs = (subs and next(subs)) and subs or false
	if not subs then return end
	for i=1,#subs do subs[i] = stack_vars_replace(subs[i]) end
	table.sort(subs, vars_sort_by_name)
end

local function locate_to_file(fname, line)
	if docs.open(fname, line) then return end

	fname = puss._path .. '/' .. fname
	if docs.open(fname, line) then return end
end

local debugger_events = {}

local function debugger_events_dispatch(co, sk, ...)
	if co then
		if sk then
			print('debugger resume:', ...)
			puss.async_service_resume(co, sk, ...)
		else
			print('debugger recved:', co, ...)
		end
	else
		local h = debugger_events[sk]
		if not h then return print('unknown stub:', sk) end
		print('debugger event:', sk, ...)
		h(...)
	end
end

local debugger_rpc
do
	local RPC_TIMEOUT = 30*1000 

	local function _rpc_thread_wrap(cb, cmd, ...)
		local co, sk = puss.async_task_alarm(RPC_TIMEOUT, GROUP_KEY)
		net.send(socket, co, sk, cmd, ...)
		cb(puss.async_task_yield())
	end

	debugger_rpc = function(cb, cmd, ...)
		if not cb then
			net.send(socket, cmd, nil, cmd, ...)
		elseif socket and socket:valid() then
			puss.async_service_run(_rpc_thread_wrap, cb, cmd, ...)
		else
			cb(false, 'bad socket')
		end
	end
end

debugger_events.attached = function(pid, bps)
	print('attached', pid, bps)
	for k,v in pairs(bps) do
		local fname, line = k:match('^(.+):(%d+)$')
		print(fname, line, v)
	end
end

debugger_events.continued = function()
	stack_list_clear()
end

debugger_events.breaked = function(res)
	stack_vars = {}
	stack_list = res
	stack_current = 1
	local info = stack_list[stack_current]
	if not info then return end
	local level = info.level
	debugger_rpc(function(ok, vars)
		if ok then stack_vars_merge(level, vars) end
	end, 'fetch_stack_subs', level)
	local fname = info.source:match('^@(.+)$')
	if fname then locate_to_file(fname, info.currentline-1) end
end

local ICONS_TEX, ICON_WIDTH, ICON_HEIGHT = nil, 32, 32
pcall(function()
	local f = io.open(puss._path .. '/core/debugger_icons.png', 'rb')
	if not f then return end
	local ctx = f:read('*a')
	f:close()
	if not ctx then return end
	ICONS_TEX, ICON_WIDTH, ICON_HEIGHT = imgui.create_image(ctx)
	if __icons_tex then imgui.destroy_image(__icons_tex) end
	__icons_tex = ICONS_TEX
end)

local function tool_button(label, hint, icon)
	local clicked
	if icon and ICONS_TEX then
		local u = ICON_HEIGHT / ICON_WIDTH
		imgui.PushID(label)
		clicked = imgui.ImageButton(ICONS_TEX, 16, 16, u*(icon-1), 0, u*icon, 1)
		imgui.PopID()
	else
		clicked = imgui.Button(label)
	end
	if imgui.IsItemHovered() then
		imgui.BeginTooltip()
		imgui.Text(hint)
		imgui.EndTooltip()
	end
	return clicked
end

local function disconnect()
	if socket then
		socket:close()
		puss.async_service_group_cancel(GROUP_KEY)
		socket, _socket = nil, nil
	end

	stack_list_clear()
	use_capture_error = nil
end

local show_connect_dialog
do
	local hosts = {}

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
			if not info then
				info = {label=string.format('%s:%s (%s)', ip, port, title), ip=ip, port=tonumber(port), title=title}
				table.insert(hosts, info)
				hosts[key] = info
			end
		end
		return true
	end

	local last_update_time = puss.timestamp()
	local targets = {}

	local function update_hosts()
		local now = puss.timestamp()
		if (now - last_update_time) < 2000 then
			for i=1,64 do
				if not recver_update() then break end
			end
			return
		end
		last_update_time = now

		targets, hosts = hosts, {}

		-- trace('----'); for i,v in ipairs(targets) do trace(v.ip, v.port, v.title) end;
		table.sort(targets, function(a,b) return a.title < b.title end)
	end

	local function connect_target(ip, port)
		disconnect()
		_socket = net.connect(ip, port)
		socket = _socket
	end

	local function show_target(info)
		imgui.TreeNodeEx(v, LEAF_FLAGS, info.label)
		if not imgui.IsItemClicked() then return end
		imgui.CloseCurrentPopup()
		connect_target(info.ip, info.port)
	end

	local remote_addr_buf = imgui.CreateByteArray(64, '127.0.0.1:xxxx')

	show_connect_dialog = function()
		local activate
		update_hosts()
		if not imgui.BeginPopupModal('Connect ...') then return end
		if imgui.Button("Close") then
			imgui.CloseCurrentPopup()
		else
			imgui.SameLine()
			imgui.PushItemWidth(160)
			imgui.InputText('##Address', remote_addr_buf)
			imgui.PopItemWidth()
			imgui.SameLine()
			if imgui.Button('Connect') then
				local ip, port = remote_addr_buf:str():match('(%d+%.%d+%.%d+%.%d+):(%d+)')
				if ip then
					imgui.CloseCurrentPopup()
					connect_target(ip, port)
				end
			end
			imgui.Separator()
			for _, info in ipairs(targets) do
				show_target(info, show_local_only)
			end
		end
		imgui.EndPopup()
	end
end

local page_line_op_active = false
local debug_input_buf = imgui.CreateByteArray(1024)
local debug_input_val = nil

local function debug_toolbar()
	if socket then
		if not socket:valid() then
			disconnect()
		elseif tool_button('Disconnect', 'Disconnect', 11) then
			disconnect()
		end
	else
		if tool_button('Connect', 'Connect ...', 10) then
			imgui.OpenPopup('Connect ...')
			imgui.SetNextWindowSize(430, 320)
		end
	end
	imgui.SameLine(nil, 2)
	if tool_button('>>', 'Continue (F5)', 1) then
		debugger_rpc(nil, 'continue')
	end
	imgui.SameLine(nil, 0)
	if tool_button('|>', 'Step over (F10)', 2) then
		debugger_rpc(nil, 'step_over')
	end
	imgui.SameLine(nil, 0)
	if tool_button('=>', 'Step into (F11)', 3) then
		debugger_rpc(nil, 'step_into')
	end
	imgui.SameLine(nil, 0)
	if tool_button('<=', 'Step out (Shift+F11)', 4) then
		debugger_rpc(nil, 'step_out')
	end
	imgui.SameLine(nil, 0)
	if tool_button('><', 'Run to .. ', 5) then
		page_line_op_active = 'run_to'
	end
	imgui.SameLine(nil, 0)
	if tool_button('JMP', 'Jump to ...', 6) then
		page_line_op_active = 'jmp_to'
	end
	imgui.SameLine(nil, 2)
	do
		local r,g,b,a,icon,tip
		if use_capture_error==true then
			r,g,b,a,icon,tip = 1,0,0,1,8,'In Captured Execption'
		elseif use_capture_error==false then
			r,g,b,a,icon,tip = 0.75,0.75,0.75,0.75,7, 'Capture Execption'
		else
			r,g,b,a,icon,tip = 0.5,0.5,0.5,0.5,7, 'Capture Execption'
		end
		imgui.PushStyleColor(ImGuiCol_Text,r,g,b,a)
		if tool_button('Err', tip, icon) then
			debugger_rpc(function(ok, capture)
				if ok then use_capture_error = capture end
			end, 'capture_error')
		end
		imgui.PopStyleColor()
	end
	imgui.SameLine(nil, 0)
	if tool_button('ASM', 'print ASM to console', 12) then
		debugger_rpc(nil, 'fetch_disasm', stack_current)
	end

	if debug_input_val then
		if tool_button('Exec', 'Execute script', 9) then
			print('execute_script', debug_input_val)
			debugger_rpc(nil, 'execute_script', stack_current, debug_input_val)
		end
		imgui.SameLine(nil, 0)
		if tool_button('BP', 'Add condition breakpoint', 8) then
			page_line_op_active = 'set_bp'
		end
		imgui.SameLine(nil, 2)
		imgui.PushItemWidth(170)
	else
		imgui.PushItemWidth(220)
	end
	imgui.InputText('##脚本', debug_input_buf, ImGuiInputTextFlags_AutoSelectAll)
	imgui.PopItemWidth()
	debug_input_val = debug_input_buf:str()
	if debug_input_val=='' then debug_input_val = nil end

	show_connect_dialog()
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
			local level = clicked.level
			debugger_rpc(function(ok, vars)
				if ok then stack_vars_merge(level, vars) end
			end, 'fetch_stack_subs', level)
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

local function load_value(val)
	local f, err = load('return '..val)
	if not f then error(err) end
	return f()
end

local function draw_subs(stack_current, subs)
	for i,v in ipairs(subs) do
		local vt = v[3]
		local has_subs = has_sub_types[vt] and v.subs~=false
		imgui.PushStyleColor(ImGuiCol_Text, 0.75, 0.75, 1, 1)
		local show = imgui.TreeNodeEx(v, has_subs and FOLD_FLAGS or LEAF_FLAGS, v[4])
		imgui.PopStyleColor()
		if has_subs and imgui.IsItemClicked() then
			if v.subs==nil then
				v.subs = dummy_vars
				local idx = v[1]
				debugger_rpc(function(ok, subs)
					if not ok then return print('fetch_index_subs failed:', subs) end
					stack_vars_fill(idx, subs)
				end, 'fetch_index_subs', idx)
			end
		end
		imgui.SameLine()
		imgui.TextColored(1, 1, 0.75, 1, tostring(v[5]))
		if debug_input_val and has_modify_types[vt] then
			imgui.SameLine()
			imgui.PushID(i)
			if imgui.SmallButton('modify') then
				local ok, val = puss.trace_pcall(load_value, debug_input_val)
				if ok then
					print('modify', val, table.unpack(v))
					debugger_rpc(function(ok, var)
						print('modify_var', ok, table.unpack(var))
						if ok then stack_vars_replace(var) end
					end, 'modify_var', v[1], val)
				else
					print('error', val)
				end
			end
			imgui.PopID()
		end
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
	net.update(socket, debugger_events_dispatch)

	if shotcuts.is_pressed('debugger/step_over') then debugger_rpc(nil, 'step_over') end
	if shotcuts.is_pressed('debugger/step_into') then debugger_rpc(nil, 'step_into') end
	if shotcuts.is_pressed('debugger/step_out') then debugger_rpc(nil, 'step_out') end
	if shotcuts.is_pressed('debugger/continue') then debugger_rpc(nil, 'continue') end

	imgui.protect_pcall(debug_toolbar)
	imgui.BeginChild('##StackAndVars')
	if imgui.CollapsingHeader('Stack', ImGuiTreeNodeFlags_DefaultOpen) then
		imgui.protect_pcall(draw_stack)
	end
	if imgui.CollapsingHeader('Vars', ImGuiTreeNodeFlags_DefaultOpen) then
		imgui.protect_pcall(draw_vars)
	end
	imgui.EndChild()
	imgui.End()
end

local function main_menu()
	local active
	if not imgui.BeginMenuBar() then return end
	if imgui.Button('Reload Ctrl+F12') then puss.reload() end
	-- active, use_local_fs = imgui.Checkbox('UseLocalFS', use_local_fs)
	active, show_console_window = imgui.Checkbox('Conosle', show_console_window)
	imgui.EndMenuBar()

	if shotcuts.is_pressed('app/reload') then puss.reload() end
end

local function trigger_bp(page, line)
	local filepath = page.filepath
	print('trigger_bp', filepath, line)
	debugger_rpc(function(ok, bp, err)
		if not ok then return end
		if err then return end
		docs.margin_set(filepath, line, bp)
	end, 'set_bp', '@'..filepath, line)
end

function docs_page_before_draw(page)
	if page_line_op_active then
		local sv = page.sv
		local op = page_line_op_active
		page_line_op_active = nil
		local line = sv:LineFromPosition(sv:GetCurrentPos())
		if op=='run_to' then
			local fname = '@'..page.filepath
			debugger_rpc(nil, op, fname, line+1)
		elseif op=='jmp_to' then
			debugger_rpc(function(ok, line)
				print('jmp_to res:', ok, line)
				if ok and line then
					local info = stack_list[1]
					if info then info.currentline = line end
				end
			end, op, line+1)
		elseif op=='set_bp' then
			local cond = debug_input_val
			if not load('return ' .. cond) then
				print('bad cond')
			else
				local filepath = page.filepath
				debugger_rpc(function(ok, bp, err)
					if not ok then return end
					if err then return end
					docs.margin_set(filepath, line+1, bp)
				end, 'set_bp', '@'..filepath, line+1, cond)
			end
		end
	end
	if shotcuts.is_pressed('debugger/bp') then
		local sv = page.sv
		local line = sv:LineFromPosition(sv:GetCurrentPos())
		trigger_bp(page, line+1)
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
	imgui.DrawListAddRectFilled(0, x,y+(line-top-1)*th,x+w-72,y+(line-top)*th,0x3FFF00FF)
end

local function file_skey_fetch(filepath)
	local ok, st = puss.stat(filepath, true)
	if not ok then return end
	return string.format('%s_%s_%s', st.size, st.mtime, st.ctime)
end

function docs_page_on_load(page_after_load, filepath)
	if use_local_fs then
		local f = io.open(puss.utf8_to_local(filepath), 'r')
		if not f then return end
		local ctx = f:read('*a')
		f:close()
		return page_after_load(ctx, file_skey_fetch(filepath))
	end

	debugger_rpc(function(ok, ctx)
		if not ok then return print('fetch_file failed:', ctx) end
		return page_after_load(ctx)
	end, 'fetch_file', filepath)
end

function docs_page_on_save(page_after_save, filepath, ctx, file_skey)
	local now_file_skey = file_skey_fetch(filepath)
	if now_file_skey ~= file_skey then
		return page_after_save(false, now_file_skey)
	end
	local f = io.open(puss.utf8_to_local(filepath), 'w')
	if not f then return page_after_save(false, now_file_skey) end
	f:write(ctx)
	f:close()
	return page_after_save(true, file_skey_fetch(filepath))
end

function docs_page_on_margin_click(page, modifiers, pos, margin, line)
	print(page, modifiers, pos, margin, line)
	trigger_bp(page, line+1)
end

local EDITOR_WINDOW_FLAGS = ( ImGuiWindowFlags_NoScrollbar
	| ImGuiWindowFlags_NoScrollWithMouse
	)

local function editor_window()
	imgui.Begin("Editor", nil, EDITOR_WINDOW_FLAGS)
	pages.update()
	miniline.update()
	imgui.End()
end

local function fs_list(dir, callback)
	if use_local_fs then
		local fs, ds = puss.file_list(dir, true)
		callback(true, fs, ds)
	elseif socket and socket:valid() then
		debugger_rpc(callback, 'file_list', dir)
	else
		callback(false)
	end
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
	imgui.Begin("FileBrowser")
	filebrowser.update(fs_list)
	imgui.End()

	imgui.SetNextWindowPos(x + left_size, y + menu_size, ImGuiCond_FirstUseEver)
	imgui.SetNextWindowSize(w - left_size, h - menu_size, ImGuiCond_FirstUseEver)
	editor_window()
	debug_window()

	if show_console_window then
		show_console_window = console.update(show_console_window)
	end
end

local last_update_time = puss.timestamp()

local function do_update()
	do
		local now = puss.timestamp()
		local delta = now - last_update_time
		last_update_time = now
		puss.async_service_update(delta, 32)
	end

	thread.update()

	imgui.protect_pcall(show_main_window)

	if run_sign and imgui.should_close() then
		run_sign = false
	end
end

local function refresh_root_folders()
	filebrowser.remove_folders()
	local default_paths = {puss._path}
	for i,v in ipairs(default_paths) do
		filebrowser.append_folder(v, fs_list)
	end
end

refresh_root_folders()

__exports.init = function()
	imgui.create('Puss - Debugger', 1024, 768, 'puss_debugger.ini', function()
		local font_name = puss._path .. '/fonts/mono.ttf'
		imgui.AddFontFromFileTTF(font_name, 14, 'Chinese')
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
