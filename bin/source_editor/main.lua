-- main.lua

local glua = puss.require('puss_gtkscintilla')

setmetatable(_ENV, {__index = glua._symbols})	-- use glua symbols

local strfmt = string.format

-- utils

local function puss_loadfile(name, env)
	local path = strfmt('%s/%s', puss._path, name)
	local f, err = loadfile(path, 'bt', env)
	if not f then return f, strfmt('load script(%s) failed: %s', path, err) end
	return f
end

local function puss_dofile(name, env, ...)
	-- print(debug.traceback('show'))
	local f, err = puss_loadfile(name, env)
	if not f then error(err) end
	return f(...)
end

puss.loadfile = puss_loadfile
puss.dofile = puss_dofile

do
	local _traceback = debug.traceback
	local _logerr = print
	local _logerr_handle = function(err) _logerr(_traceback(err,2)); return err;  end
	puss.trace_pcall = function(f, ...) return xpcall(f, _logerr_handle, ...) end
	puss.trace_dofile = function(name, env, ...) return xpcall(puss_dofile, _logerr_handle, name, env or _ENV, ...) end
end

-- module

puss.__modules__ = {}
puss.__imports__ = {}

do
	local module_mt = { __index=_ENV }
	local modules = puss.__modules__
	local imports = puss.__imports__

	local function module_env_create(m)
		local env = {}
		local function global_register(name, init_value)
			assert( type(init_value)=='table' or type(init_value)=='userdata', 'global register MUST table or userdata' )
			local value = env[name] or init_value
			env[name] = value
			return value
		end
		env.__global_register = global_register
		return setmetatable(env, module_mt)
	end

	local module_ensure = function(name)
		local m = modules[name]
		if not m then
			m = { name=name, args={} }
			m.env = module_env_create(m)
			modules[name] = m
		end
		return m
	end

	local function module_dofile(name, filename, script, ...)
		local m = module_ensure(name)
		return puss_dofile(filename, m.env, ...)
	end

	puss.module_import = function(name, ...)
		local i = imports[name]
		-- print('module_import', name, i)
		if i then return i end
		i = {}
		imports[name] = i
		local m = module_ensure(name)
		local env = m.env
		m.args = table.pack(...)
		print('start  dofile:', name)
		puss_dofile(name, env, ...)
		print('finish dofile:', name)
		setmetatable(i, {__index=env, __newindex=function() error('module readonly!') end})
		return i
	end
end

-- init

local app = puss.module_import('source_editor/app.lua')

function __main__()
	app.main()
end

