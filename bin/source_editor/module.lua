-- module.lua

local function create_exports()
	local wrappers = {}
	
	local function create_export(name, method)
		local wrapper = puss.function_wrap(method)
		rawset(_ENV, name, wrapper)
		wrappers[name] = wrapper
		return wrapper
	end
	
	local mt = {}
	mt.__call = function(t, k)
		local wrapper = wrappers[k]
		return wrapper and puss.function_wrap(nil, wrapper)
	end
	mt.__index = function(t, k)
		return wrappers[k] or create_export(k, function() error('method('..tostring(k)..') not export') end)
	end
	mt.__newindex = function(t, k, v)
		assert( type(v)=='function', 'export MUST function' )
		local wrapper = wrappers[k]
		if not wrapper then 
			wrapper = create_export(k, v)
		elseif wrapper==v then
			print('export self wrapper error:', k)
		else
			puss.function_wrap(v, wrapper)
		end
		return wrapper
	end
	
	return setmetatable({}, mt)
end

-- module
puss._modules = puss._modules or {}

local modules = puss._modules

puss.import = function(name)
	local env = modules[name]
	if env then return getmetatable(env).__exports end
	local exports = create_exports()
	env = setmetatable({_name=name, _exports=exports}, {__index=_ENV, __name=name, __exports=exports})
	modules[name] = env
	local fname = string.format('source_editor/%s.lua', name)
	-- print('import', fname, 'begin')
	puss.dofile(fname, env)
	-- print('import', fname, 'end')
	return exports
end

