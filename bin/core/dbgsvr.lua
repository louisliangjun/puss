-- host debug server
-- 
local puss_debug = __puss_debug__
local sock = puss.import('core.sock')

-- inject debug utils into host
-- 
puss_debug:__host_pcall('puss.trace_dostring', [[

puss._debug_fetch_stack = function()
	local infos = {}
	for level=3,256 do
		local info = debug.getinfo(level, 'Slut')
		if not info then break end
		info.level = level
		table.insert(infos, info)
	end
	return infos
end

puss._debug_fetch_vars = function(level)
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
function MT:fetch_stack() return self:__host_pcall('puss._debug_fetch_stack') end
function MT:fetch_vars(level) return level, self:__host_pcall('puss._debug_fetch_vars', level) end

local listen_sock = sock.listen(nil, 9999)
local socket, address
local send_breaked_frame

local function dispatch(cmd, ...)
	print( 'host recv:', cmd, ... )
	if not cmd:match('^%w[_%w]+$') then error('host command name('..tostring(cmd)..') error!') end
	local handle = puss_debug[cmd]
	if not handle then error('host unknown cmd('..tostring(cmd)..')') end
	return sock.send(socket, cmd, handle(puss_debug, ...))
end

local function hook_main_update(breaked, frame)
	if not socket then
		socket, address = sock.accept(listen_sock)
		if socket then
			print('host attach', socket, address)
		else
			breaked = false
		end
	elseif not socket:valid() then
		print('host detach', socket, address)
		breaked, socket, address = false, nil, nil
	else
		if breaked and send_breaked_frame ~= frame then
			sock.send(socket, 'breaked')
			send_breaked_frame = frame
		end
		sock.update(socket, dispatch)
	end

	if not breaked then
		send_breaked_frame = nil
		puss_debug:continue()
	end
end

return puss_debug:__reset(hook_main_update, 8192)

