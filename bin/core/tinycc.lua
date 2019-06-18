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

local function debug_on_fetch_tcc_stabs(ok, descs)
	print(tccdbg, ok, descs)
	if not ok then return end
	if not tccdbg then return end
	local stabs = tccdbg:set_data('stabs', {})
	for name, desc in pairs(descs) do
		-- print('  ', name, stab)
		stabs[name] = tccdbg:parse_stab(desc)
	end
	for name, stab in pairs(stabs) do
		print('stab:', name)
		for i, sym in ipairs(stab) do
			print(i, table.unpack(sym))
		end
	end
end

__exports.debug_attach = function(pid, rpc)
	tccdbg = puss_tinycc.debug_attach(pid)
	_tccdbg = tccdbg
	if not tccdbg then return end
	rpc(debug_on_fetch_tcc_stabs, 'fetch_tcc_stabs')
end

__exports.debug_detach = function()
	if not tccdbg then return end
	tccdbg:detach()
	tccdbg, _tccdbg = nil, nil
end

__exports.debug_update = function(dispatch)
	while tccdbg do
		if not tccdbg:wait(dispatch, 1) then break end
	end
end

puss._debug_fetch_tcc_stabs = function()
	local res = {}
	for name, module in pairs(modules) do
		res[name] = module:fetch_stab()
	end
	return res
end
