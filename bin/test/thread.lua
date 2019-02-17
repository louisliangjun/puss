-- test/thread.lua

local function test1()
	local t = puss.thread_create(false, 'puss.dostring', [[
		print('start', ...)
		local step = 0
		assert( puss.thread_signal==nil )
		assert( puss.thread_wait==nil )
		while step < 3 do
			step = step + 1
			print('sleep 3s', step, ...)
			puss.sleep(3000)
		end
		return print('exit', step, ...)
	]], nil, 't1')

	t:join()
end

local function test2()
	local t = puss.thread_create('puss.dostring', [[
		print('start', ...)
		local step = 0
		while step < 5 do
			step = step + 1
			print('sleep 3s', step, ...)
			puss.thread_wait(3000)
		end
		return print('exit', step, ...)
	]], nil, 't2')

	for i=1,8 do
		puss.sleep(1000)
		t:post('print', '  [THREAD] print', i)
	end

	t:join()
end

local function test3()
	local t = puss.thread_create('puss.dostring', [[
		print('start', ...)
		local step = 0
		puss.thread_signal(function(...)
			print('  [EV]', ...)
			step = step + 1
		end)
		while step < 5 do
			step = step + 1
			print('sleep 3s', step, ...)
			puss.thread_wait(3000)
		end
		return print('exit', step, ...)
	]], nil, 't3')

	for i=1,10 do
		puss.sleep(1000)
		t:post('event', i)
	end

	t:join()
end

local function test4(enable_wait)
	local q = puss.queue_create()
	local t = puss.thread_create('puss.dostring', [[
		local name, q = ...
		print('start', name)
		local step = 0
		puss.thread_signal(function(...)
			print('  [EV]', ...)
			step = step + 1
		end)
		while step < 10 do
			step = step + 1
			print('sleep 10s', step, name)
			local msg, a,b,c = q:pop(10000)
			if msg then
				print('  [MSG]', msg, a,b,c)
			end
		end
		return print('exit', step, name)
	]], nil, 't4', q)

	for i=1,5 do
		puss.sleep(1000)
		t:post('event', i)
		puss.sleep(1000)
		q:push('msg', i)
	end

	t:join()
end

local function test5(enable_wait)
	local n = 10000000
	local q = puss.queue_create()
	local qc = puss.queue_create()
	local s = [[
		local name, q, qc = ...
		print('start', name)
		qc:pop(10000000)
		print('recv start')
		local ts = puss.timestamp()
		local i = 0
		while q:pop(100) do
			i = i + 1
		end
		local te = puss.timestamp()
		return print('exit', name, i, te-ts)
	]]
	local t1 = puss.thread_create('puss.dostring', s, nil, 't5.1', q, qc)
	local t2 = puss.thread_create(false, 'puss.dostring', s, nil, 't5.2', q, qc)

	do
		puss.sleep(500)
		local ts = puss.timestamp()
		for i=1,n do
			q:push(i)
		end
		local te = puss.timestamp()
		print('send use', te-ts)
		qc:push('start_recv 1')
		qc:push('start_recv 2')
	end

	t1:join()
	t2:join()
end

function __main__()
	print(pcall(test1))
	print(pcall(test2))
	print(pcall(test3))
	print(pcall(test4))
	print(pcall(test5))
	os.execute('pause')
end
