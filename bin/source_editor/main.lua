-- main.lua

puss.dofile('source_editor/module.lua', _ENV)

local glua = puss.require('puss_gtkscintilla')
setmetatable(_ENV, {__index = glua._symbols})	-- use glua symbols

local APP = puss.import('app')

function __main__()
	APP.main()
end

