-- default.lua

_ENV.imgui = puss.require('puss_imgui')	-- MUST require before require other modules who use imgui

local modules = {}
local modules_base_mt = { __index=_ENV }

local function load_module(name, env)
	puss.dofile(name:gsub('%.', puss._sep) .. '.lua', env)
end

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
	load_module(name, env)
	return interface
end

puss.reload = function()
	for name, env in pairs(modules) do
		puss.trace_pcall(load_module, name, env)
	end
end

function __main__()
	local puss_app = puss.import('core.app')
	puss_app.init()
	while puss_app.update() do
	end
	puss_app.uninit()
end

