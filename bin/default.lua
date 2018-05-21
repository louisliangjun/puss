-- default.lua

_ENV.imgui = puss.require('puss_imgui')

puss._modules = puss._modules or {}
puss._modules_base_mt = puss._modules_base_mt or { __index=_ENV }

local modules = puss._modules
local modules_base_mt = puss._modules_base_mt

puss.import = function(name, reload)
	local env = modules[name]
	local exports
	if env then
		exports = getmetatable(env).__exports
		if not reload then return exports end
	else
		exports = {}
		local module_env = setmetatable({__name=name, __exports=exports}, modules_base_mt)
		module_env.__index = module_env
		env = setmetatable({}, module_env)
		modules[name] = env
	end

	-- print('import', name, 'begin')
	local fname = name:gsub('%.', '/') .. '.lua'
	puss.dofile(fname, env)
	-- print('import', name, 'end')
	return exports
end

function __main__()
	local puss_app = puss.import('core.app')
	puss_app.main()
end

