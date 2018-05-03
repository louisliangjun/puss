-- debugger.lua

print('debugger', puss._script)

-- host debug server
-- 
if puss._script~='tools/debugger.lua' then
	if not puss.debug then
		return print('ERROR : need run application with --debug')
	end

	local puss_socket = puss.require('puss_socket')

	local current_debugger
	local sock, addr = puss_socket.socket_udp_create('127.0.0.1', 9999, 1*1024*1024)
	print('host listen at:', addr)

	local function host_debug_update()
		local res, msg, from = sock:recvfrom()
		if not msg then return end
		current_debugger = from
		print( msg )
		if msg=='step_into' then
			puss.debug.step_into()
		elseif msg=='continue' then
			puss.debug.continue()
		end
	end

	local event_callbacks = {}

	event_callbacks[PUSS_DEBUG_EVENT_ATTACHED] = function()
		print('attached')
	end

	event_callbacks[PUSS_DEBUG_EVENT_UPDATE] = function()
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_HOOK_COUNT] = function()
		-- print('count')
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_BEGIN] = function()
		print('breaked begin')
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_UPDATE] = function()
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_END] = function()
		print('breaked end')
	end

	puss.debug.reset(function(ev) event_callbacks[ev]() end, 512)
	return
end

-- debugger boot
-- 
if not imgui then
	local imgui = puss.require('puss_imgui')
	_ENV.imgui = imgui
	setmetatable(_ENV, {__index=imgui})
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
function puss_debugger_ui(sock, source_view)
	-- ShowUserGuide()
	-- ShowDemoWindow()

	Begin("Puss Debugger")
	if Button("connect") then
		sock:connect('127.0.0.1', 9999)
	end
	if Button("step_into") then
		sock:send('step_into')
	end
	if Button("continue") then
		sock:send('continue')
	end
	scintilla_update(source_view)
	End()
end

function __main__()
	local puss_socket = puss.require('puss_socket')
	local sock = puss_socket.socket_udp_create()
	local main_window = glfw_imgui_create("puss debugger", 1024, 768)
	local source_view = source_view_create()

	while main_window:update(puss_debugger_ui, sock, source_view) do
		main_window:render()
	end

	main_window:destroy()
end

