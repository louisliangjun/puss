-- module.lua

-- NOTICE : puss.function_wrap()
--   wrapper = puss.function_wrap(method)       -- create wrapper
--   method  = puss.function_wrap(nil, wrapper) -- fetch method from wrapper
--   puss.function_wrap(new_methods, wrapper)   -- replace wrapper method

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
		elseif wrapper~=v then
			puss.function_wrap(v, wrapper)
		else
			error('export recursion wrapper dead loop error:'..tostring(k))
		end
		return wrapper
	end

	return setmetatable({}, mt)
end

-- module
puss._modules = puss._modules or {}

local modules = puss._modules

puss.import = function(name, reload)
	local env = modules[name]
	local exports
	if env then
		exports = getmetatable(env).__exports
		if not reload then return exports end
	else
		exports = create_exports()
		env = setmetatable({_name=name, _exports=exports}, {__index=_ENV, __name=name, __exports=exports})
		modules[name] = env
	end

	local fname = string.format('source_editor/%s.lua', name)
	-- print('import', fname, 'begin')
	puss.dofile(fname, env)
	-- print('import', fname, 'end')
	return exports
end

