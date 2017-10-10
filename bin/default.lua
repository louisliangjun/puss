-- default.lua

table.trace = function(t)
	local keys = {}
	local table_insert = table.insert
	for k,v in pairs(t) do table_insert(keys, k) end
	table.sort(keys)
	for _, k in ipairs(keys) do print(k, t[k]) end
	print('>>>> table keys:', #keys) 
end

local function _trace_test()
	print('============== puss');	table.trace(puss)

	if false then
		local glua = puss.require('puss_gobject')
		print('============== glua', glua);	table.trace(glua)
		print('============== glua._gtypes', glua._gtypes);	table.trace(glua._gtypes)
		print('============== glua._types', glua._types);	table.trace(glua._types)
		print('============== glua._symbols', glua._symbols);	table.trace(glua._symbols)
	end

	if false then
		local glua = puss.require('puss_gtk')
		print('============== glua', glua);	table.trace(glua)
		print('============== glua._types', glua._types);	table.trace(glua._types)
		print('============== glua._symbols', glua._symbols);	table.trace(glua._symbols)
	end

	if false then
		local glua = puss.require('puss_gtksourceview')
		print('============== glua', glua);	table.trace(glua)
		print('============== glua._types', glua._types);	table.trace(glua._types)
		print('============== glua._symbols', glua._symbols);	table.trace(glua._symbols)
	end

	if true then
		local glua = puss.require('puss_gtkscintilla')
		print('============== glua', glua);	table.trace(glua)
		print('============== glua._types', glua._types);	table.trace(glua._types)
		print('============== glua._symbols', glua._symbols);	table.trace(glua._symbols)
	end

	if true then
		local enums = puss._consts
		print('============== puss._consts', puss._consts);	table.trace(puss._consts)

		print('NOTICE: current file already compiled, so no enums replace, GTK_MAJOR_VERSION:', tostring(GTK_MAJOR_VERSION))
		print('NOTICE: if current file want use enums, use puss._consts.GTK_MAJOR_VERSION:', puss._consts.GTK_MAJOR_VERSION)
		load("print('NOTICE: new compiled file/string support, GTK_MAJOR_VERSION:', GTK_MAJOR_VERSION)", 'string')()
	end
end

function __main__()
	_trace_test()
end

