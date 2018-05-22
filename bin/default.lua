-- default.lua

_ENV.imgui = puss.require('puss_imgui')

local modules = {}
local modules_base_mt = { __index=_ENV }

puss.import = function(name, reload)
	local env = modules[name]
	local interface
	if env then
		interface = getmetatable(env)
		if not reload then return interface end
	else
		interface = {__name=name }
		interface.__exports = interface
		interface.__index = interface
		local module_env = setmetatable(interface, modules_base_mt)
		env = setmetatable({}, module_env)
		modules[name] = env
	end
	puss.dofile(name:gsub('%.', '/') .. '.lua', env)
	return interface
end

function __main__()
	local puss_app = puss.import('core.app')
	puss_app.main()
end

