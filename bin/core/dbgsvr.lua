-- host debug server
-- 
local puss_debug = __puss_debug__

puss_debug:__host_pcall('puss.trace_dostring', '\n\n\n\n'..[[

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

puss._debug_current_vars = {}	-- array of {'local|upvalue|vaarg', 'name', value}

local function do_ret(vars, ps, pe)
	local ret = {}
	for i=ps,pe do
		local v = vars[i]
		local vt, vv = type(v[3]), tostring(v[3])
		if vt=='string' then vv = string.format('%q', vv) end
		table.insert(ret, {i, v[1], tostring(v[2]), vv})
	end
	return ret
end
puss._debug_fetch_vars = function(level)
	local info = debug.getinfo(level, 'uf')
	local vars = {}
	puss._debug_current_vars = vars
	for i=1,255 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		if n:match('^%(.+%)$')==nil then table.insert(vars, {'local', n, v}) end
	end
	for i=1,255 do
		local n,v = debug.getupvalue(info.func, i)
		if not n then break end
		table.insert(vars, {'upvalue', n, v})
	end
	for i=-1,-255,-1 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		table.insert(vars, {'vaarg', n, v})
	end
	return do_ret(vars, 1, #vars)
end

puss._debug_fetch_table = function(i)
	local vars = puss._debug_current_vars
	local v = vars[i]
	if not v then return end
	if v==vars then return end
	local start = #vars
	local vt, vv = type(v[3]), tostring(v[3])
	if vt~='table' then return i end
	for xk, xv in pairs(vt) do
		table.insert(vars, {'field', xk, xv})
	end
	return do_ret(vars, start, #vars)
end

]], '<DebugInject>')

local net = puss.import('core.net')

local MT = getmetatable(puss_debug)
function MT:fetch_stack() return self:__host_pcall('puss._debug_fetch_stack') end
function MT:fetch_vars(level) return level, self:__host_pcall('puss._debug_fetch_vars', level) end
function MT:fetch_table(level, i) return level, i, self:__host_pcall('puss._debug_fetch_table', i) end

local listen_sock = net.listen(nil, 9999)
local socket, address
local send_breaked_frame

local function dispatch(cmd, ...)
	print( 'host recv:', cmd, ... )
	if not cmd:match('^%w[_%w]+$') then error('host command name('..tostring(cmd)..') error!') end
	local handle = puss_debug[cmd]
	if not handle then error('host unknown cmd('..tostring(cmd)..')') end
	return net.send(socket, cmd, handle(puss_debug, ...))
end

local function hook_main_update(breaked, frame)
	if not socket then
		socket, address = net.accept(listen_sock)
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
			net.send(socket, 'breaked')
			send_breaked_frame = frame
		end
		net.update(socket, dispatch)
	end

	if not breaked then
		send_breaked_frame = nil
		puss_debug:continue()
	end
end

return puss_debug:__reset(hook_main_update, 8192)

