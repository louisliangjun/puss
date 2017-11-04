puss.dofile('samples/utils.lua')

function __main__()
	local nk = puss.require('puss_nuklear')
	print(nk.application_run)
	nk.application_run(nk.test1)
end

