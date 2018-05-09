-- debugger.lua

-- host debug server
-- 
local puss_debug = __puss_debug__

if puss_debug then
	local MT = getmetatable(puss_debug)

	MT.dostring = function(self, script)
		local f, e = load(script, '<debug-host-dostring>')
		if not f then error(e) end
		return f()
	end

	local function dispatch(cmd, ...)
		-- print( 'recv result:', cmd, ... )
		local handle = puss_debug[cmd]
		if not handle then
			error('unknown cmd('..tostring(cmd)..')')
		end
		return handle(puss_debug, ...)
	end

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
function puss_debugger_ui(source_view)
	-- ShowUserGuide()
	-- ShowDemoWindow()

	Begin("Puss Debugger")
	if puss.debug then
		if Button("show_debugger") then
			os.execute('start /B ' .. puss._path .. '/' .. puss._self .. ' tools/debugger.lua')
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
		sock:send(puss.pickle_pack('step_into'))
	end
	SameLine()
	if Button("continue") then
		sock:send(puss.pickle_pack('continue'))
	end
	SameLine()
	if Button("dostring") then
		local n, s = source_view:GetText(source_view:GetTextLength())
		local src = puss.pickle_pack('host_pcall', 'puss.trace_dostring', s)
		-- print(puss.pickle_unpack(src))
		sock:send(src)
	end
	ScintillaUpdate(source_view)
	End()
end

function __main__()
	local main_window = ImGuiCreateGLFW("puss debugger", 400, 300)
	local source_view = source_view_create()
	source_view:SetText([[print('hello', puss_imgui)]])

	while true do
		local ok, running = puss.trace_pcall(ImGuiUpdate, main_window, puss_debugger_ui, source_view)
		if ok and (not running) then break end
		ImGuiRender(main_window)
	end

	ScintillaFree(source_view)
	ImGuiDestroy(main_window)
end

