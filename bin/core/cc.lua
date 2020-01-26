-- core.cc

local PUSS_INC_PATH = puss._path .. '/../include'
local TINYCC_ROOT_PATH = puss._path..'/../tinycc/'..puss.OS

local tinycc =
	{ path = TINYCC_ROOT_PATH
	, cc = TINYCC_ROOT_PATH..'/tcc'
	, ar = TINYCC_ROOT_PATH..'/tcc'
	}

local function array_pack(...)
	local arr = {}
	local function _pack(t)
		if not t then return end
		if type(t)~='table' then return table.insert(arr, t) end
		for i,v in ipairs(t) do _pack(v) end -- array
	end
	local n = select('#', ...)
	for i=1,n do _pack(select(i, ...)) end
	return arr
end

local function array_convert(arr, convert)
	local outs = {}
	for _, v in ipairs(arr) do
		local o = convert(v)
		if o then table.insert(outs, o) end
	end
	return outs
end

-- args_concat('a', 'b', 'c') ==> 'a b c'
-- args_concat( {'a', 'b'}, 'c') ==> 'a b c'
-- args_concat('a', {'b'}, {'c'}) ==> 'a b c'
-- 
local function args_concat(...)
	return table.concat(array_pack(...), ' ')
end

local function shell(...)
	local cmd = args_concat(...)
	local p = io.popen(cmd)
	local r = p:read('*a'):match('^%s*(.-)%s*$')
	p:close()
	return r
end

__exports.build_plugin = function(toolchain, plugin_name, srcs)
	local target = puss._plugin_prefix .. plugin_name .. puss._plugin_suffix
	-- os.remove(target)
	shell(toolchain.cc, '-shared'
		, '-I'..PUSS_INC_PATH
		, '-o', target
		, srcs
		)
end

__exports.build_plugin_sample = function()
	build_plugin(tinycc, 'sample_plugin', puss._path..'/test/sample_plugin.c')
end

--[[
puss.import('core.cc').build_plugin_sample()
local diskfs = puss.import('core.diskfs')
local plugin_name = puss._plugin_prefix..'sample_plugin'..puss._plugin_suffix
mm = mm or puss.load_plugin_mpe('sample_plugin', diskfs.load(plugin_name))
print(mm)
print(mm.add(3,4))
print(mm.div(9,2))
--]]
