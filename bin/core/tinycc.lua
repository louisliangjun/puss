-- tinycc.lua

local puss_tinycc = puss.load_plugin('puss_tinycc')

local TCC_GEN = false

local C_ROOT_PATH = puss._path .. '/core/c'

local TCC_OPTIONS = '-Wall -Werror -nostdinc -nostdlib -I'..C_ROOT_PATH..' -D'..(IS_SERVER and '_SERVER' or '_CLIENT')
if puss.debug then
	TCC_OPTIONS = '-g -D_DEBUG ' .. TCC_OPTIONS
else
	TCC_OPTIONS = '-g -DNDEBUG ' .. TCC_OPTIONS
end

__caches__ = __caches__ or {}
local caches = __caches__

__modules__ = __modules__ or {}
local modules = __modules__

puss_tinycc.tcc_load_file = function(name)
	local ctx = caches[name]
	if ctx then return ctx end
	local f = io.open(C_ROOT_PATH..'/'..name, 'r')
	if not f then return end
	ctx = f:read('*a')
	f:close()
	return ctx
end

local function module_reply(name, tcc, ...)
	modules[name] = tcc
	return ...
end

local function module_build(tcc, name, srcs, ...)
	tcc:set_options(TCC_OPTIONS)
	tcc:set_output_type('memory')
	if type(srcs)=='table' then
		for _, src in ipairs(srcs) do
			tcc:add_file(src)
		end
	else
		tcc:add_file(srcs)
	end
	tcc:add_file('libtcc1.c')
	-- tcc:add_symbols(SYMBOLS)
	return module_reply(name, tcc, tcc:relocate('__module_init', ...))
end

__exports.load_module = function(srcs, ...)
	local name = type(srcs)=='table' and srcs[1] or srcs
	return puss_tinycc.tcc_load_module(module_build, name, srcs, ...)
end

__exports.add_file = function(name, context)
	caches[name] = context
end

-- debugger & debuggee

local tccdbg = _tccdbg
local tccdbg_events = {}

local function debug_trace(...)
	print('[TCCDBG]', ...)
end

local function trace_stab(stab, trace)
	local ptr, ptx = stab.ptr, stab.ptx
	local offset = (ptx & 0xFFFFFFFF00000000)
	local faddr = 0	-- function addr
	local stack = 0
	for i, sym in ipairs(stab) do
		local t, s, v, d = table.unpack(sym)
		if t=='SLINE' then
			trace(string.format('%d	SLINE	0x%016x	%d', stack, faddr + v, d))
		elseif t=='FUN' then
			if not s then	-- end of fun
				faddr = 0
			else			-- begin of fun
				faddr = offset + v
				trace(string.format('%d	FUNCTION	0x%016x', stack, faddr + v))
			end
		elseif t=='SO' then
			if not s then	-- end of source
				stack = 0
			elseif #s>0 and s:sub(-1)~='/' then
				stack = stack + 1
				trace(string.format('%d	SOURCE	%s', stack, puss.filename_format(s)))
			else
				trace(string.format('%d	SO	%s', stack, s))
			end
		elseif t=='BINCL' then	-- begin include
			stack = stack + 1
			trace(string.format('%d	INCLUDE	%s', stack, puss.filename_format(s)))
		elseif t=='EINCL' then	-- end include
			stack = stack - 1
		end
	end
	return stab
end

local function debug_on_fetch_tcc_stabs(ok, descs)
	print(tccdbg, ok, descs)
	if not ok then return end
	if not tccdbg then return end
	local stabs = tccdbg:set_data('stabs', {})
	for name, desc in pairs(descs) do
		-- debug_trace('  ', name, stab)
		stabs[name] = tccdbg:parse_stab(desc)
		trace_stab(stabs[name], debug_trace)
	end
end

tccdbg_events.create_process = function()
end

tccdbg_events.process_exit = function()
end

tccdbg_events.breakpoint = function(first_chance)
	debug_trace('breaked')
	tccdbg:cont()
	debug_trace('continue')
end

tccdbg_events.single_step = function(first_chance)
	debug_trace('breaked - single-step')
	tccdbg:cont()
	debug_trace('continue - single-step')
end

tccdbg_events.exception = function(first_chance, emsg, code, addr)
	debug_trace('exception', first_chance)
	if first_chance then
		tccdbg:cont(true)
		debug_trace('continue - not handled')
	else
		tccdbg:cont()
		debug_trace('continue')
	end
end

tccdbg_events.debug_string = function(msg)
	debug_trace('debug_string', msg)
end

local function tccdbg_events_dispatch(dbg, ev, ...)
	debug_trace(ev, ...)
	puss.trace_pcall(tccdbg_events[ev], ...)
end

__exports.debug_attach = function(pid, rpc)
	tccdbg = puss_tinycc.debug_attach(pid)
	_tccdbg = tccdbg
	print('tcc debug attach to', pid, tccdbg)
	if not tccdbg then return end
	rpc(debug_on_fetch_tcc_stabs, 'fetch_tcc_stabs')
end

__exports.debug_detach = function()
	if not tccdbg then return end
	tccdbg:detach()
	tccdbg, _tccdbg = nil, nil
end

__exports.debug_update = function()
	while tccdbg do
		if not tccdbg:wait(tccdbg_events_dispatch, 1) then break end
	end
end

puss._debug_fetch_tcc_stabs = function()
	local res = {}
	for name, module in pairs(modules) do
		res[name] = module:fetch_stab()
	end
	return res
end
