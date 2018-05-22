-- default.lua

_ENV.imgui = puss.require('puss_imgui')

local modules = {}
local modules_base_mt = { __index=_ENV }

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
	puss.dofile(name:gsub('%.', '/') .. '.lua', env)
	return exports
end

function __main__()
	local puss_app = puss.import('core.app')
	puss_app.main()
end

