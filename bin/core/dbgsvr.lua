-- host debug server
-- 
local puss_debug = __puss_debug__

-- inject debug utils into host
puss_debug:host_pcall('puss.trace_dostring', [[
puss._debug = {}
puss._debug.fetch_stack = function()
	local infos = {}
	for level=3,256 do
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
		local n,v = debug.getlocal(level, i)
		if not n then break end
		if n~='(*temporary)' then locals[n] = tostring(v) end
	end
	for i=1,255 do
		local n,v = debug.getupvalue(info.func, i)
		if not n then break end
		ups[n] = tostring(v)
	end
	for i=-1,-255,-1 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		varargs[n] = tostring(v)
	end
	return locals, ups, varargs
end
]], '<DebugInject>')

local MT = getmetatable(puss_debug)

function MT:fetch_stack()
	return self:host_pcall('puss._debug.fetch_stack')
end

function MT:fetch_vars(level)
	return level, self:host_pcall('puss._debug.fetch_vars', level)
end

local sock = puss.import('core.sock')

local listen_sock = sock.listen(nil, 9999)
local socket
local address
local send_breaked_frame

local function dispatch(cmd, ...)
	print( 'host recv:', cmd, ... )
	local handle = puss_debug[cmd]
	if not handle then
		error('host unknown cmd('..tostring(cmd)..')')
	end
	return sock.send(socket, cmd, handle(puss_debug, ...))
end

local function on_update()
	if socket then
		if socket:valid() then
			sock.update(socket, dispatch)
		else
			print('host detach', socket, address)
			socket, address = nil, nil
			send_breaked_frame = nil
			puss_debug:continue()
		end
	else
		socket, address = sock.accept(listen_sock)
		if socket then
			print('host attach', socket, address)
		else
			send_breaked_frame = nil
			puss_debug:continue()
		end
	end
end

return puss_debug:reset(function(breaked, frame)
	if breaked and socket  then
		if send_breaked_frame ~= frame then
			sock.send(socket, 'breaked')
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

