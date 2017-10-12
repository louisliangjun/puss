puss.dofile('samples/utils.lua')

function __main__()
	print(puss, require('puss'))
	table.trace(puss, 'puss')
	table.trace(puss._args, 'puss._args')
end

