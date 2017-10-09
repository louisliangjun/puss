-- default.lua

function __main__()
	print('run puss default main')

	for k,v in pairs(puss) do print(k,v) end

	print(puss.require('puss_gobject'))
	print(glua)
end
