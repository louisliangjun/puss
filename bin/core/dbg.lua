-- dbg.lua

local shotcuts = puss.import('core.shotcuts')
local docs = puss.import('core.docs')

local sock = _sock
local stack_list = {}
local stack_clicked = nil
local stack_current = 1

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

local function debug_toolbar()
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

local function fetch_test_data()
	local a = 3
	local b = 4
	local c = (a + b) .. 'test'

	-- test use current stack
	if next(stack_list)==nil then
		local infos = {}
		for level=1,256 do
			local info = debug.getinfo(level, 'Slut')
			if not info then break end
			info.level = level
			table.insert(infos, info)
		end

		stack_list = infos
	end

	if stack_clicked then
		stack_current = stack_clicked
		stack_clicked = nil
		local info = stack_list[stack_current]
		if info then
			local locals, ups, varargs = {}, {}, {}
			info.locals = locals
			info.ups = ups
			info.varargs = varargs

			local level = info.level
			local info = debug.getinfo(level, 'uf')
			for i=1,255 do
				local n,v = debug.getlocal(level, i)
				if not n then break end
				locals[n] = tostring(v)
			end
			for i=1,255 do
				local n,v = debug.getupvalue(info.func, i)
				if not n then break end
				ups[n] = tostring(v)
			end
			for i=-1,-255,-1 do
				local n = debug.getlocal(level, i)
				if not n then break end
				varargs[n] = tostring(v)
			end
		end
	end
end

local function draw_stack()
	for i,info in ipairs(stack_list) do
		local label = string.format('%s:%d', info.short_src, info.currentline)
		imgui.TreeNodeEx(tostring(info), ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
		if imgui.IsItemClicked() then stack_clicked = i end
	end
end

local function draw_vars()
	local info = stack_list[stack_current]
	if not info then
		return imgui.Text('<empty>')
	end
	if info.locals then
		for n,v in pairs(info.locals) do
			local label = string.format('%s: %s', n, v)
			imgui.TreeNodeEx(label, ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, label)
			if imgui.IsItemClicked() then print(label) end
		end
	end
end

__exports.update = function(main_ui)
	fetch_test_data()

	main_ui:protect_pcall(debug_toolbar)
	if imgui.CollapsingHeader('Stack', ImGuiTreeNodeFlags_DefaultOpen) then
		main_ui:protect_pcall(draw_stack)
	end
	if imgui.CollapsingHeader('Vars', ImGuiTreeNodeFlags_DefaultOpen) then
		main_ui:protect_pcall(draw_vars)
	end
end

