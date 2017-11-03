puss.dofile('samples/utils.lua')

function __main__()
	local nk = puss.require('puss_nuklear_glfw3')
	print(nk.test)
	nk.test(nk.test1)
end

