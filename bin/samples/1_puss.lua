dofile('samples/z_utils.lua')

function __main__()
	table.trace(puss, 'puss')
	table.trace(puss._args, 'puss._args')
end

