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
	local f, err = puss_loadfile(name, env)
	if not f then
		print(debug.traceback( strfmt('loadfile(%s) failed!', name) ))
		error(err)
	end
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
puss_dofile('source_editor/module.lua', _ENV)

-- init

local APP = puss.import('app')

function __main__()
	APP.main()
end

