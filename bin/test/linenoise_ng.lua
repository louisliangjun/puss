local linenoise = puss.load_plugin('puss_linenoise_ng')

function __main__()
	-- linenoise.PrintKeyCodes()

	linenoise.SetCompletionCallback(function(s)
		local last = s:match('.-(%w+)$')
		if not last then return end
		local n = #last
		for k in pairs(_G) do
			if #k > n and k:sub(1,n)==last then
				linenoise.AddCompletion(s..k:sub(n+1))
			end
		end
	end)

	for i=1,100 do
		local line = linenoise.ReadLine('\x1b[01;31mlinenoise\x1b[0m> ')
		if line then
			print(puss.utf8_to_local(line))
			linenoise.HistoryAdd(line)
		end
	end
end
