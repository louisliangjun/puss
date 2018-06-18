-- debugger.lua

if not __puss_debug__ then
	-- debugger boot
	-- 
	_ENV.imgui = puss.require('puss_imgui')	-- MUST require before require other modules who use imgui
	_ENV.puss_socket = puss.require('puss_socket')

	function __main__()
		local debugger = puss.import('core.debugger')
		debugger.init()
		while debugger.update() do
			imgui.WaitEventsTimeout()
		end
		debugger.uninit()
	end

	return
end

-- host debug server
-- 
local puss_debug = __puss_debug__
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
local sock, addr
local send_breaked_frame

print('puss_socket.socket_create()', listen_sock)
print('debug noblock:', listen_sock:set_nonblock(true))
print('debug bind:', listen_sock:bind(nil, 9999))
print('debug listen:', listen_sock:listen())

local function send_to_debugger_client(cmd, ...)
	sock:send(puss.pickle_pack(cmd, ...))
end

local function dispatch(cmd, ...)
	print( 'recv result:', cmd, ... )
	local handle = puss_debug[cmd]
	if not handle then
		error('unknown cmd('..tostring(cmd)..')')
	end
	return send_to_debugger_client(cmd, handle(puss_debug, ...))
end

local function on_update()
	if sock then
		local res, msg = sock:recv()
		if res < 0 then
			if msg==10035 then	-- WSAEWOULDBLOCK(10035)	-- TODO : now win32 test
				return
			end
			-- WSAECONNRESET(10054)
			print('recv error:', res, msg)
			sock:close()
			sock, addr = nil, nil
			send_breaked_frame = nil
		elseif res==0 then
			print('detach:', sock, addr)
			sock:close()
			sock, addr = nil, nil
			send_breaked_frame = nil
		else
			dispatch(puss.pickle_unpack(msg))
		end
	else
		sock, addr = listen_sock:accept()
		if sock then
			print('attached:', sock, addr)
			send_breaked_frame = nil
			sock:set_nonblock(true)
		end
	end
end

return puss_debug:reset(function(breaked, frame)
	if breaked and sock then
		if send_breaked_frame ~= frame then
			send_to_debugger_client('breaked')
			send_breaked_frame = frame
		else
			-- breaked idle
		end
	else
		puss_debug:continue()
		send_breaked_frame = nil
	end
	on_update()
end, 8192)

