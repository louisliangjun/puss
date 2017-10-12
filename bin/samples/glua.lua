puss.dofile('samples/utils.lua')

function __main__()
	local glua = puss.require('puss_gobject')	-- 'puss_gobject', 'puss_gtksourceview', 'puss_gtkscintilla'
	print(glua)
	table.trace(glua, 'glua')
	table.trace(glua._types, 'glua._types')
	table.trace(glua._symbols, 'glua._symbols')
end

