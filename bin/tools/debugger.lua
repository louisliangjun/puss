-- debugger.lua

-- host debug server
-- 
local puss_debug = __puss_debug__

if puss_debug then
	local MT = getmetatable(puss_debug)

	-- inject debug utils into host
	puss_debug:host_pcall('puss.trace_dostring', [[
puss._debug = {}
puss._debug.fetch_stack = function()
	local infos = {}
	for level=1,256 do
		local info = debug.getinfo(level, 'Slut')
		if not info then break end
		info.level = level
		table.insert(infos, info)
	end
	return infos
end
puss._debug.fetch_vars = function(level)
	local locals, ups, varargs = {}, {}, {}
	local info = debug.getinfo(level, 'uf')
	for i=1,255 do
		local v = debug.getlocal(level, i)
		if not v then break end
		table.insert(locals, v)
	end
	for i=1,255 do
		local v = debug.getupvalue(info.func, i)
		if not v then break end
		table.insert(ups, v)
	end
	for i=-1,-255,-1 do
		local v = debug.getlocal(level, i)
		if not v then break end
		table.insert(varargs, v)
	end
	return locals, ups, varargs
end
	]], '<DebugInject>')

	local puss_socket = puss.require('puss_socket')
	local listen_sock = puss_socket.socket_create()
	print('puss_socket.socket_create()', listen_sock)
	print('debug bind:', listen_sock:bind(nil, 9999))
	print('debug listen:', listen_sock:listen())

	puss_debug:reset(function(...)
		print('event handle:', ...)
	end, 8192)

	local sock, addr = listen_sock:accept()
	print('debug accept:', sock, addr)

	if not sock then
		print('accept failed:', sock, addr)
		return
	end

	local function response(cmd, ...)
		sock:send(puss.pickle_pack(cmd, ...))
	end

	local function dispatch(cmd, ...)
		-- print( 'recv result:', cmd, ... )
		local handle = puss_debug[cmd]
		if not handle then
			error('unknown cmd('..tostring(cmd)..')')
		end
		return response(cmd, handle(puss_debug, ...))
	end

	print('attached:', sock, addr)

	while sock do
		local res, msg = sock:recv()
		if res < 0 then
			print('recv error:', res, msg)
		elseif res==0 then
			break	-- disconnected
		else
			dispatch(puss.pickle_unpack(msg))
		end
	end

	print('detach:', sock, addr)
	return
end

-- debugger boot
-- 
if not puss_imgui then
	local puss_imgui = puss.require('puss_imgui')
	local puss_socket = puss.require('puss_socket')
	_ENV.puss_imgui = puss_imgui
	_ENV.puss_socket = puss_socket
	setmetatable(_ENV, {__index=puss_imgui})
	puss.dofile(puss._script, _ENV)
	return
end

local source_view_create
do
	local lua_keywords = [[
     and       break     do        else      elseif    end
     false     for       function  goto      if        in
     local     nil       not       or        repeat    return
     then      true      until     while
     self     _ENV
	]]

	source_view_create = function()
		local sv = ScintillaNew()

		sv:SetTabWidth(4)
		sv:SetLexerLanguage('lua')
		sv:SetKeyWords(0, lua_keywords)

		sv:StyleSetFore(SCE_LUA_DEFAULT, 0x00000000)
		sv:StyleSetFore(SCE_LUA_COMMENT, 0x00808080)
		sv:StyleSetFore(SCE_LUA_COMMENTLINE, 0x00008000)
		sv:StyleSetFore(SCE_LUA_COMMENTDOC, 0x00008000)
		sv:StyleSetFore(SCE_LUA_NUMBER, 0x07008000)
		sv:StyleSetFore(SCE_LUA_WORD, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_STRING, 0x001515A3)
		sv:StyleSetFore(SCE_LUA_CHARACTER, 0x001515A3)
		sv:StyleSetFore(SCE_LUA_LITERALSTRING, 0x001515A3)
		sv:StyleSetFore(SCE_LUA_PREPROCESSOR, 0x00808080)
		sv:StyleSetFore(SCE_LUA_OPERATOR, 0x7F007F00)
		sv:StyleSetFore(SCE_LUA_IDENTIFIER, 0x00000000)
		sv:StyleSetFore(SCE_LUA_STRINGEOL, 0x001515A3)
		sv:StyleSetFore(SCE_LUA_WORD2, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD3, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD4, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD5, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD6, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD7, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_WORD8, 0x00FF0000)
		sv:StyleSetFore(SCE_LUA_LABEL, 0x0000FF00)

		return sv
	end
end

local function source_view_set_file(source_view, fname)
	local pth = puss._path .. '/' .. fname
	local f = io.open(pth)
	if not f then return end

	local t = f:read('*a')
	f:close()

	source_view:SetText(t)
	source_view:EmptyUndoBuffer()
end

-- debugger client
-- 
local function debug_call(cmd, ...)
	sock:send(puss.pickle_pack(cmd, ...))
	local res, msg = sock:recv()
	if res < 0 then
		print('debugger recv error:', res, msg)
	elseif res==0 then
		print('debugger disconnected')
	else
		print('debugger recv:', puss.pickle_unpack(msg))
	end
end

function puss_debugger_ui(source_view)
	-- ShowUserGuide()
	-- ShowDemoWindow()

	Begin("Puss Debugger")
	if puss.debug then
		if Button("show_debugger") then
			if os.getenv('OS')=='Windows_NT' then
				os.execute('start /B ' .. puss._path .. '/' .. puss._self .. ' tools/debugger.lua')
			else
				os.execute(puss._path .. '/' .. puss._self .. ' tools/debugger.lua &')
			end
		end
		SameLine()
		if Button("start_debug_self") then
			puss.debug()
		end
	end

	if Button("connect") then
		if sock then sock:close() end
		sock = puss_socket.socket_create()
		sock:connect('127.0.0.1', 9999)
	end
	SameLine()
	if Button("step_into") then
		debug_call('step_into')
	end
	SameLine()
	if Button("continue") then
		debug_call('continue')
	end
	SameLine()
	if Button("dostring") then
		local n, s = source_view:GetText(source_view:GetTextLength())
		debug_call('host_pcall', 'puss.trace_dostring', s)
	end
	SameLine()
	if Button("stack") then
		local n, s = source_view:GetText(source_view:GetTextLength())
		debug_call('host_pcall', 'puss._debug.fetch_stack')
	end
	ScintillaUpdate(source_view)
	End()
end

function __main__()
	local main_window = ImGuiCreateGLFW("puss debugger", 400, 300)
	local source_view = source_view_create()

	source_view:SetText([[print('===')
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

	while true do
		local ok, running = puss.trace_pcall(ImGuiUpdate, main_window, puss_debugger_ui, source_view)
		if ok and (not running) then break end
		ImGuiRender(main_window)
	end

	ScintillaFree(source_view)
	ImGuiDestroy(main_window)
end

