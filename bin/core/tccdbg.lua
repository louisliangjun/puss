-- tccdbg.lua

local puss_tinycc = puss.load_plugin('puss_tinycc')

function __main__()
	local dbg = puss_tinycc.debug_attach(7800)
	print(dbg)
	while true do
		print(dbg:wait(2000))
	end
	puss.sleep(5000)
end

