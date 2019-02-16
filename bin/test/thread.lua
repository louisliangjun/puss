-- test.thread

if select(1, ...) then
	print('start', ...)
	local sign = 0
	puss.thread_signal(function(sig)
		print('  got sig', sig, sign)
		sign = sign + 1
	end)
	while sign < 5 do
		print('sleep 5', ...)
		puss.sleep(5000)
	end
	return print('exit', ...)
end

local threads =
	{ puss.thread_create('puss.dofile', puss._path..'/test/thread.lua', nil, '__thread11__')
	, puss.thread_create('puss.dofile', puss._path..'/test/thread.lua', nil, '__thread22__')
	}

__exports.kill = function(i)
	local t = threads[i]
	print(t:kill())
end

__exports.exist = function(i)
	local t = threads[i]
	print(t:exist())
end
