-- test/thread.lua

local function test1()
	local t = puss.thread_create(false, 'puss.dostring', [[
		print('start', ...)
		print('sleep 3s', ...)
		puss.sleep(3000)
		print('wait 3s', ...)
		print('wait result:', puss.thread_wait(3000))
		return print('exit', ...)
	]], nil, 't1')

	t:join()
end

local function test2()
	local t = puss.thread_create('puss.dostring', [[
		print('start', ...)
		local step = 0
		while step < 4 do
			step = step + 1
			print('wait 5s', step, ...)
			if puss.thread_wait(5000) then break end
		end
		return print('exit', step, ...)
	]], nil, 't2')

	for i=1,2 do
		puss.sleep(1000)
		t:post('print', '  break [THREAD] print', i)
	end

	print('main sleep 3s')
	puss.sleep(3000)
	t:join()
end

local function test3()
	local t = puss.thread_create('puss.dostring', [[
		print('start', ...)
		local step = 0
		local function event_handle(...)
			print('  [EV]', ...)
			step = step + 1
		end
		while step < 5 do
			step = step + 1
			print('wait 3s', step, ...)
			print('wait result:', puss.thread_wait(event_handle, 3000))
		end
		return print('exit', step, ...)
	]], nil, 't3')

	for i=1,2 do
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
		local function event_handle(...)
			print('  [EV]', ...)
			step = step + 1
		end
		while step < 6 do
			step = step + 1
			local msg, i = q:pop()
			if msg then
				print('  [MSG]', msg, i)
			end
			print('wait 10s', step, name)
			print('wait result:', puss.thread_wait(event_handle, 10000, q))
		end
		return print('exit', step, name)
	]], nil, 't4', q)

	for i=1,2 do
		puss.sleep(1000)
		t:post('event', i)
		puss.sleep(1000)
		q:push('msg', i)
	end

	print('main sleep 3s')
	puss.sleep(3000)
	t:join()
end

local function test5(enable_wait)
	local n = 1000000
	local q = puss.queue_create()
	local s = [[
		local name, q, ms = ...
		print('start', name)
		puss.thread_wait(10000000)
		print('recv start', name)
		local ts = puss.timestamp()
		local i = 0
		while q:pop(100) do
			i = i + 1
		end
		local te = puss.timestamp()
		puss.sleep(ms)
		return print('exit', name, i, te-ts)
	]]
	local t1 = puss.thread_create('puss.dostring', s, nil, 't5.1', q, 100)
	local t2 = puss.thread_create('puss.dostring', s, nil, 't5.2', q, 200)

	do
		puss.sleep(500)
		local ts = puss.timestamp()
		for i=1,n do
			q:push(i)
		end
		local te = puss.timestamp()
		print('send use', te-ts)
		
		t1:post('print', 'start_recv 1')
		t2:post('print', 'start_recv 2')
	end

	t1:join()
	t2:join()
end

function __main__()
	print(pcall(test1), '\n'); collectgarbage();
	print(pcall(test2), '\n'); collectgarbage();
	print(pcall(test3), '\n'); collectgarbage();
	print(pcall(test4), '\n'); collectgarbage();
	print(pcall(test5), '\n'); collectgarbage();

	if puss.OS=='win32' then os.execute('pause') end
end
