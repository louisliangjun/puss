local ng = puss.load_plugin('puss_linenoise_ng')

function __main__()
	for i=1,100 do
		local line = ng.linenoise('PS:')
		if line then
			print(puss.utf8_to_local(line))
			ng.linenoiseHistoryAdd(line)
		end
	end
end
