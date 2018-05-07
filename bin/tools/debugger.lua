-- debugger.lua

-- host debug server
-- 
if __puss_debug__ then
	local puss_debug = __puss_debug__
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
			local cmd, a1, a2, a3 = puss.pickle_unpack(msg)
			print( 'recv result:', res, cmd, a1, a2, a3 )
			if cmd=='step_into' then
				puss_debug:step_into()
			elseif cmd=='continue' then
				puss_debug:continue()
			elseif cmd=='host_pcall' then
				puss_debug:host_pcall(a1, a2, a3)
			end
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
		local sv = scintilla_new()

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
	if Button("connect") then
		if sock then sock:close() end
		sock = puss_socket.socket_create()
		sock:connect('127.0.0.1', 9999)
	end
	if Button("step_into") then
		sock:send(puss.pickle_pack('step_into'))
	end
	if Button("continue") then
		sock:send(puss.pickle_pack('continue'))
	end
	if Button("host_pcall") then
		local n, s = source_view:GetText(source_view:GetTextLength())
		local src = puss.pickle_pack('host_pcall', 'print', s)
		print(puss.pickle_unpack(src))
		sock:send(puss.pickle_pack('host_pcall', 'print', s))
	end
	scintilla_update(source_view)
	End()
end

function __main__()
	local main_window = glfw_imgui_create("puss debugger", 1024, 768)
	local source_view = source_view_create()

	while main_window:update(puss_debugger_ui, source_view) do
		main_window:render()
	end

	main_window:destroy()
end

