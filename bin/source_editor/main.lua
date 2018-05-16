-- main.lua

-- import 3rd dependence

local puss_imgui = puss.require('puss_imgui')
_ENV.imgui = puss_imgui

-- modules

puss._modules = puss._modules or {}
puss._modules_base_mt = puss._module_base_mt or {__index=_ENV, __name='PussModuleBaseMT'}

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

	local fname = string.format('source_editor/%s.lua', name)
	-- print('import', fname, 'begin')
	puss.dofile(fname, env)
	-- print('import', fname, 'end')
	return exports
end

-- main

local puss_app = puss.import('app')

function __main__()
	puss_app.main()
end

