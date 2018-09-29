-- host debug server
-- 
if puss._debug_proxy then
	local puss_system = puss.require('puss_system')
	local net = puss.import('core.net')
	local puss_debug = puss._debug_proxy
	do
		local ok, res = puss_debug:__host_pcall('puss.trace_dofile', puss._debug_filename)
		if not ok then print('DebugInject failed:', res) end
	end

	local MT = getmetatable(puss_debug)
	function MT:fetch_stack() return self:__host_pcall('puss._debug_fetch_stack') end
	function MT:fetch_vars(level) return level, self:__host_pcall('puss._debug_fetch_vars', level) end
	function MT:fetch_subs(idx) return idx, self:__host_pcall('puss._debug_fetch_subs', idx) end
	function MT:modify_var(idx, val) return self:__host_pcall('puss._debug_modify_var', idx, val) end

	local broadcast_udp = puss_system.socket_new()
	broadcast_udp:create(AF_INET, puss_system.SOCK_DGRAM, puss_system.IPPROTO_UDP)
	broadcast_udp:bind()

	local listen_sock, listen_addr = net.listen(nil, 0, true)
	local socket, address
	local send_breaked_frame

	local BROADCAST_IP = '127.0.0.255'
	local BROADCAST_PORT = 9999
	local BROADCAST_INFO = listen_addr
	do
		local ok, title = puss_debug:__host_pcall('puss._debug_fetch_title')
		if ok then BROADCAST_INFO = string.format('%s|%s', listen_addr, title) end
	end

	local function dispatch(cmd, ...)
		-- print( 'host recv:', cmd, ... )
		if not cmd:match('^%w[_%w]+$') then error('host command name('..tostring(cmd)..') error!') end
		local handle = puss_debug[cmd]
		if not handle then error('host unknown cmd('..tostring(cmd)..')') end
		return net.send(socket, cmd, handle(puss_debug, ...))
	end

	local function hook_main_update(breaked, frame)
		if not socket then
			broadcast_udp:sendto(BROADCAST_IP, BROADCAST_PORT, BROADCAST_INFO)
			print('broadcast', BROADCAST_IP, BROADCAST_PORT, BROADCAST_INFO)

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

local lua_type_unknown = {'?', tostring}
local lua_types = {}

local function lua_type_desc_of(v)
	local tt = lua_types[type(v)] or lua_type_unknown
	return tt[1], tt[2](v)
end

local function desc_of_string(v)
	local s = string.format('%q', v)
	if #s > 128 then
		s = s:sub(1,128) .. '...'
	end
	return s
end

local function desc_of_table(t)
	local out = {}
	local mt = getmetatable(t)
	local prefix = ''
	if mt then
		if mt.__tostring then
			prefix = tostring(t)
		elseif mt.__name then
			prefix = string.format('<%s>', mt.__name)
		end
	end
	local c = 3
	if #t > 0 then
		for i,v in ipairs(t) do
			if c <= 0 then break end
			c = c - 1
			table.insert(out, tostring(v))
		end
	else
		for k,v in pairs(t) do
			if c <= 0 then break end
			c = c - 1
			table.insert(out, string.format('%s=%s', tostring(k), tostring(v)))
		end
	end
	if c <= 0 then table.insert(out, '...') end
	local ctx = table.concat(out, ',')
	if #ctx > 128 then ctx = ctx:sub(1,128)..'...' end
	return prefix..'{'..ctx..'}'
end

lua_types =
	{ ['nil'] = {'-', function(v) return 'nil' end}
	, ['string'] = {'S', desc_of_string}
	, ['boolean'] = {'B', function(v) return v end}
	, ['number'] = {'N', function(v) return v end}
	, ['table'] = {'T', desc_of_table}
	, ['userdata'] = {'U', tostring}
	, ['thread'] = {'H', tostring}
	, ['function'] = {'F', tostring}
	}

local function do_ret_one(idx, var)
	local s, n, v = var[1], var[2], var[3]
	local vt, vv = lua_type_desc_of(v)
	return {idx, s, vt, tostring(n), vv}
end

local function do_ret(vars, ps, pe)
	local ret = {}
	for i=ps,pe do
		table.insert(ret, do_ret_one(i, vars[i]))
	end
	return ret
end

local function var_modify_local(var, idx, val)
	local level, i = var[4], var[5]
	debug.setlocal(level, i, v)
	local n, v = debug.getlocal(level, i)
	var[3] = v
	return do_ret_one(idx, var)
end

local function var_modify_upvalue(var, idx, val)
	local level, i = var[4], var[5]
	local info = debug.getinfo(level, 'uf')
	debug.setupvalue(info.func, i, val)
	local n, v = debug.getupvalue(info.func, i)
	var[3] = v
	return do_ret_one(idx, var)
end

local function var_modify_table(var, idx, val)
	local t = val[4]
	rawset(t, val)
	var[3] = val
	return do_ret_one(idx, var)
end

local function var_modify_uservalue(var, idx, val)
	local u = val[4]
	debug.setuservalue(u, val)
	var[3] = debug.getuservalue(u)
	return do_ret_one(idx, var)
end

puss._debug_fetch_vars = function(level)
	local vars = puss._debug_stack_vars
	local key = string.format('stack:%d', level)
	local range = vars[key]
	if range then return do_ret(vars, range[1], range[2]) end
	local info = debug.getinfo(level, 'uf')
	local start = #vars+1
	for i=1,255 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		if n:match('^%(.+%)$')==nil then table.insert(vars, {'L', n, v, level, i, modify=var_modify_local}) end
	end
	for i=1,255 do
		local n,v = debug.getupvalue(info.func, i)
		if not n then break end
		table.insert(vars, {'U', n, v, level, i, modify=var_modify_upvalue})
	end
	for i=-1,-255,-1 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		table.insert(vars, {'V', n, v, level, i, modify=var_modify_local})
	end
	vars[key] = {start, #vars}
	return do_ret(vars, start, #vars)
end

puss._debug_fetch_subs = function(idx)
	local vars = puss._debug_stack_vars
	local start = #vars + 1
	local var = vars[idx]
	if not var then return end
	print('fetch_subs:', idx, var, table.unpack(var))
	if var==vars then return end	-- no cache
	local value = var[3]
	local vt = type(value)
	if vt=='table' then
		local range = vars[value]
		if range then return do_ret(vars, range[1], range[2]) end
		local mt = getmetatable(value)
		if mt then table.insert(vars, {'M', '@metatable', mt}) end
		for k, v in pairs(value) do table.insert(vars, {'T', k, v, value, modify=var_modify_table}) end
	elseif vt=='userdata' then
		local range = vars[value]
		if range then return do_ret(vars, range[1], range[2]) end
		local mt = getmetatable(value)
		if mt then table.insert(vars, {'M', '@metatable', mt}) end
		local uv = debug.getuservalue(value)
		if uv then table.insert(vars, {'P', '@uservalue', uv, value, modify=var_modify_uservalue}) end
	else
		return
	end
	vars[value] = {start, #vars}
	return do_ret(vars, start, #vars)
end

puss._debug_modify_var = function(idx, val)
	local var = vars[idx]
	if not var then return end
	if not var.modify then return end
	return var.modify(var, idx, val)
end

puss._debug_fetch_title = function()
	return puss._app_title or 'noname'
end

