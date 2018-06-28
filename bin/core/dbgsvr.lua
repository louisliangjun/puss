-- host debug server
-- 
if puss._debug_proxy then
	local net = puss.import('core.net')
	local puss_debug = puss._debug_proxy
	do
		local ok, res = puss_debug:__host_pcall('puss.trace_dofile', puss._debug_filename)
		if not ok then print('DebugInject failed:', res) end
	end

	local MT = getmetatable(puss_debug)
	function MT:fetch_stack() return self:__host_pcall('puss._debug_fetch_stack') end
	function MT:fetch_vars(level) return level, self:__host_pcall('puss._debug_fetch_vars', level) end
	function MT:fetch_subs(key) return key, self:__host_pcall('puss._debug_fetch_subs', key) end

	local listen_sock = net.listen(nil, 9999)
	local socket, address
	local send_breaked_frame

	local function dispatch(cmd, ...)
		-- print( 'host recv:', cmd, ... )
		if not cmd:match('^%w[_%w]+$') then error('host command name('..tostring(cmd)..') error!') end
		local handle = puss_debug[cmd]
		if not handle then error('host unknown cmd('..tostring(cmd)..')') end
		return net.send(socket, cmd, handle(puss_debug, ...))
	end

	local function hook_main_update(breaked, frame)
		if not socket then
			socket, address = net.accept(listen_sock)
			-- print('accept', listen_sock, socket, address)
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
				-- print('host breaked', frame)
				net.send(socket, 'breaked')
				send_breaked_frame = frame
				puss_debug:__host_pcall('puss._debug_on_breaked')
			end
			net.update(socket, dispatch)
		end

		if not breaked then
			send_breaked_frame = nil
			puss_debug:continue()
		end
	end

	return puss_debug:__reset(hook_main_update, 8192)
end

-- debug helper

-- array of {scope, 'name', value}
-- scope L:local U:upvalue V:vaarg F:field M:metatable P:uservalue ...
-- 
puss._debug_stack_vars = {}

puss._debug_on_breaked = function()
	-- print('clear debug')
	puss._debug_stack_vars = {}
end

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

local lua_types =
	{ ['nil'] = '-'
	, ['string'] = 'S'
	, ['boolean'] = 'B'
	, ['number'] = 'N'
	, ['table'] = 'T'
	, ['userdata'] = 'U'
	, ['thread'] = 'H'
	, ['function'] = 'F'
	}

local function do_ret(vars, ps, pe)
	if ps > pe then return end
	local ret = {}
	for i=ps,pe do
		local var = vars[i]
		local vt, vv = type(var[3]), tostring(var[3])
		if vt=='string' then vv = string.format('%q', vv) end
		table.insert(ret, {i, var[1], lua_types[vt] or '?', tostring(var[2]), vv})
	end
	return ret
end

puss._debug_fetch_vars = function(level)
	local info = debug.getinfo(level, 'uf')
	local vars = puss._debug_stack_vars
	local start = #vars+1
	for i=1,255 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		if n:match('^%(.+%)$')==nil then table.insert(vars, {'L', n, v}) end
	end
	for i=1,255 do
		local n,v = debug.getupvalue(info.func, i)
		if not n then break end
		table.insert(vars, {'U', n, v})
	end
	for i=-1,-255,-1 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		table.insert(vars, {'V', n, v})
	end
	return do_ret(vars, start, #vars)
end

puss._debug_fetch_subs = function(i)
	local vars = puss._debug_stack_vars
	local start = #vars + 1
	local var = vars[i]
	if not var then return end
	if var==vars then return end	-- no cache
	local value = var[3]
	local vt = type(value)
	if vt=='table' then
		local mt = getmetatable(value)
		if mt then table.insert(vars, {'M', '@metatable', mt}) end
		for k, v in pairs(value) do table.insert(vars, {'F', k, v}) end
	elseif vt=='userdata' then
		local mt = getmetatable(value)
		if mt then table.insert(vars, {'M', '@metatable', mt}) end
		local uv = debug.getuservalue(value)
		if uv then table.insert(vars, {'P', '@uservalue', uv}) end
	end
	return do_ret(vars, start, #vars)
end

