-- host debug server
-- 
if puss._debug_proxy then
	print('dbgsvr load:', puss._debug_proxy, ...)

	local net = puss.import('core.net')
	local puss_debug = puss._debug_proxy
	local mt =  getmetatable(puss_debug)
	do
		local ok, res = puss_debug:__host_pcall('puss.trace_dofile', puss._debug_filename)
		if not ok then print('* DebugInject failed:', res) end
	end

	mt.clear_bps = function(self)
		local bps = puss_debug:get_bps()
		for k,v in pairs(bps) do
			local fname, line = k:match('^(.+):(%d+)$')
			puss_debug:set_bp(fname, tonumber(line), false)
		end
	end

	local DEBUG_TITLE, WAIT_TIMEOUT = ...

	local BROADCAST_PORT = 7654
	local broadcast = net.create_udp_broadcast_sender(BROADCAST_PORT)

	local listen_socket, host_info
	local socket, address

	local function do_reply(cmd, co, sk, ok, ...)
		if not ok then print('* host run(', cmd, 'error:', ...) end
		net.send(socket, co, sk, ok, ...)
	end

	local function dispatch(co, sk, cmd, ...)
		-- print( '* host recv:', cmd, ... )
		if not cmd:match('^%w[_%w]+$') then
			return do_reply(cmd, co, sk, false, 'host command name('..tostring(cmd)..') error!')
		end
		local h = puss_debug[cmd]
		if h then return do_reply(cmd, co, sk, pcall(h, puss_debug, ...)) end
		return do_reply(cmd, co, sk, puss_debug:__host_pcall('puss._debug_'..cmd, ...))
	end

	local BROADCAST_INTERVAL = 200
	local broadcast_timeout = puss.timestamp() + BROADCAST_INTERVAL

	local function broadcast_hostinfo()
		if puss.timestamp() < broadcast_timeout then return end
		broadcast_timeout = puss.timestamp() + BROADCAST_INTERVAL

		if not listen_socket then
			local s, a = net.listen(nil, 0, true)
			if not s then return end
			listen_socket, host_info = s, string.format('[PussDebug]|%s', a)
			host_info = string.format('%s|%s', host_info, DEBUG_TITLE)
			print('* debug listen at', host_info)
		end

		broadcast(host_info)
		-- print('* broadcast', BROADCAST_PORT, host_info)
	end

	local CHECK_INTERVAL = 200
	local check_timeout = puss.timestamp() + CHECK_INTERVAL

	local function check_client_connect(wait_time)
		if puss.timestamp() < check_timeout then return end
		check_timeout = puss.timestamp() + CHECK_INTERVAL

		if not listen_socket then return end
		socket, address = net.accept(listen_socket, wait_time)
		-- print('* accept', listen_socket, socket, address)
		if not socket then return false end
		print('* host attach', socket, address)
		listen_socket:close()
		listen_socket, host_info = nil, nil
		net.send(socket, nil, 'attached', puss_debug:getpid(), puss_debug:get_bps())
		return true
	end

	local function hook_main_update(step)
		if not socket then
			if step==0 then
				broadcast_hostinfo()
				if check_client_connect() then
					puss_debug:__reset(hook_main_update, true, 8192)	-- attached
					return
				end
			end
			puss_debug:continue()
			return
		end

		if step < 0 then
			local ok, stack = puss_debug:__host_pcall('puss._debug_fetch_stack')
			--print('* host breaked', socket, address)
			net.send(socket, nil, 'breaked', ok and stack or {})
			puss_debug:__host_pcall('puss.__on_breaked')
		elseif step > 0 then
			--print('* host continued', socket, address)
			net.send(socket, nil, 'continued')
		else
			if socket:valid() then
				net.update(socket, dispatch)
			else
				print('* host detach', socket, address)
				puss_debug:__reset(hook_main_update, false, 8192)	-- detached
				socket, address = nil, nil
				puss_debug:continue()
			end
		end
	end

	puss_debug:__reset(hook_main_update, false, 8192)	-- init

	if type(WAIT_TIMEOUT)=='number' then
		local wait_connect_timeout = puss.timestamp() + (WAIT_TIMEOUT*1000)
		print('* debug wait',  WAIT_TIMEOUT)
		while puss.timestamp() < wait_connect_timeout do
			broadcast_hostinfo()
			if check_client_connect(100) then
				puss_debug:__reset(hook_main_update, true, 8192)
				puss_debug:step_into()	-- pause
				break
			end
		end
		print('* debug wait finished!')
	end

	return
end

-- debug helper

puss._debug_fetch_stack = function()
	local infos = {}
	for level=3,256 do
		local info = debug.getinfo(level, 'Slutn')
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
	debug.setlocal(level, i, val)
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
	local t, k = var[4], var[2]
	rawset(t, k, val)
	var[3] = val
	return do_ret_one(idx, var)
end

local function var_modify_uservalue(var, idx, val)
	local u = var[4]
	debug.setuservalue(u, val)
	var[3] = debug.getuservalue(u)
	return do_ret_one(idx, var)
end

local function fetch_table_elems(vars, value)
	local array = {}
	local sorted = {}
	for i in ipairs(value) do
		table.insert(array, i)
	end
	for k in pairs(value) do
		if not array[k] then table.insert(sorted, k) end
	end
	table.sort(sorted, function(a, b) return tostring(a) < tostring(b) end)
	for _, k in ipairs(array) do local v = rawget(value, k); table.insert(vars, {'T', k, v, value, modify=var_modify_table}) end
	for _, k in ipairs(sorted) do local v = rawget(value, k); table.insert(vars, {'T', k, v, value, modify=var_modify_table}) end
end

-- array of {scope, 'name', value}
-- scope L:local U:upvalue V:vaarg F:field M:metatable P:uservalue ...
-- 
local vars = {}

puss.__on_breaked = function()
	-- print('* clear debug')
	vars = {}
end

puss._debug_fetch_stack_subs = function(level)
	local start = #vars + 1
	local key = string.format('stack:%d', level)
	local range = vars[key]
	if range then return do_ret(vars, range[1], range[2]) end
	local info = debug.getinfo(level, 'uf')
	for i=1,255 do
		local n,v = debug.getlocal(level, i)
		if not n then break end
		if n:match('^%(.+%)$')==nil then table.insert(vars, {'L', n, v, level, i, modify=var_modify_local}) end
	end
	for i=1,info.nups do
		local n,v = debug.getupvalue(info.func, i)
		table.insert(vars, {'U', n, v, level, i, modify=var_modify_upvalue})
	end
	if info.isvararg then
		for i=-1,-255,-1 do
			local n,v = debug.getlocal(level, i)
			if not n then break end
			table.insert(vars, {'V', n, v, level, i, modify=var_modify_local})
		end
	end
	range = {start, #vars}
	vars[key] = range
	return do_ret(vars, range[1], range[2])
end

local function fetch_value_subs(value)
	local start = #vars + 1
	local vt = type(value)
	if vt~='table' and vt~='userdata' then return end
	local range = vars[value]
	if range then return range end
	local mt = getmetatable(value)
	if mt then table.insert(vars, {'M', '@metatable', mt}) end
	if vt=='table' then
		pcall(fetch_table_elems, vars, value)
	else
		local uv = debug.getuservalue(value)
		if uv then table.insert(vars, {'P', '@uservalue', uv, value, modify=var_modify_uservalue}) end
	end
	table.insert(vars, {'H', nil, value})
	local range = {start, #vars - 1, ref = #vars}
	vars[value] = range
	return range
end

puss._debug_fetch_index_subs = function(idx)
	local var = vars[idx]
	if not var then return end
	if var==vars then return end	-- no cache
	local range = fetch_value_subs(var[3])
	return do_ret(vars, range[1], range[2])
end

puss._debug_modify_var = function(idx, val)
	--print('* host modify', idx, val)
	local var = vars[idx]
	if not var then return end
	if not var.modify then return end
	return var.modify(var, idx, val)
end

puss._debug_execute_script = function(level, script)
	--print('* host execute_script', level, script)
	local info = debug.getinfo(level + 2, 'f')
	local function get_from_env(key)
		local k, v = debug.getupvalue(info.func, 1)
		if k == '_ENV' then return v[key] end
	end
	local function ul_read_wrapper(t, key)
		for i=1,255 do
			local k, v = debug.getlocal(level + 3, i)
			if not k then break end
			if k == key then return v end
		end
		for i=1,255 do
			local k, v = debug.getupvalue(info.func, i)
			if not k then break end
			if k == key then return v end
		end
		local ok, res = pcall(get_from_env, key)
		if ok and res~=nil then return res end
		return _G[key]	-- NOTICE: for debugger test
	end
	local function ul_write_wrapper(t, key, val)
		for i=1,255 do
			local k, v = debug.getlocal(level + 3, i)
			if not k then break end
			if k == key then debug.setlocal(level + 3, i, val) return end
		end
		for i=1,255 do
			local k, v = debug.getupvalue(info.func, i)
			if not k then break end
			if k == key then debug.setupvalue(info.func, i, val) return end
		end
		error('can not find local or upvalue in this CallInfo')
	end
	local env = setmetatable({}, {__index = ul_read_wrapper, __newindex = ul_write_wrapper})
	local f, errcode = load(script, '<debug-script>', 't', env)
	if not f then error(errcode) end
	return f()
end

puss._debug_hover = function(level, script)
	local v = puss._debug_execute_script(level + 1, 'return ' .. script)
	local vt, vv = lua_type_desc_of(v)
	local ref
	if vt=='T' or vt=='U' then
		local range = vars[v] or fetch_value_subs(v)
		ref = range.ref
	end
	return vt, vv, ref
end

puss._debug_fetch_file = function(filepath)
	local f = io.open(puss.utf8_to_local(filepath), 'r')
	if not f then return end
	local ctx = f:read('*a')
	f:close()
	return ctx
end

puss._debug_list_dir = function(filepath)
	return puss.file_list(dir, true)
end
