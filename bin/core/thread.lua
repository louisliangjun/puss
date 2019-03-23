-- core.thread

local REPLY_QUERY = 1
local REPLY_ASYNC = 2

local replys = {}

local thread_name, request_queue, response_queue = ...

if thread_name then
	replys[REPLY_QUERY] = function(module_name, handle_name, pt, ...)
		if not handle_name then return end
		response_queue:push(REPLY_QUERY, module_name, handle_name, ...)
	end

	replys[REPLY_ASYNC] = function(co, sk, pt, ...)
		response_queue:push(REPLY_ASYNC, co, sk, false, ...)
	end

	local function handle_request(reply_mode, k1, k2, pt, ...)
		if not reply_mode then return end
		local reply = replys[reply_mode]
		local stub = _G[pt]
		if type(stub)~='function' then
			reply(k1, k2, pt, false, string.format('pt(%s) not found', pt))
		else
			reply(k1, k2, pt, puss.trace_pcall(stub, ...))
		end
	end

	local function wait_request()
		handle_request(request_queue:pop())
	end

	puss.thread_notify = function(module_name, handle_name, ...)
		response_queue:push(REPLY_QUERY, module_name, handle_name, ...)
	end

	puss.import('core.thread_search_file')

	local last_update_time = puss.timestamp()
	local wait_timeout = 100

	local function idle()
		puss.trace_pcall(wait_request)

		local now = puss.timestamp()
		local delta = now - last_update_time
		last_update_time = now
		local more, first_timeout = puss.async_service_update(delta, 32)
		if first_timeout < 0 then
			first_timeout = 0
		elseif first_timeout > 1000 then
			first_timeout = 1000
		end
		wait_timeout = first_timeout
	end

	if puss.debug then
		puss.async_service_run(function()
			puss.debug(true, nil, thread_name)
			while true do
				puss.debug()
				puss.async_task_sleep(50)
			end
		end)
	end

	repeat
		puss.trace_pcall(idle)
	until puss.thread_wait(handle_request, wait_timeout, request_queue)

	return print(thread_name, 'thread exit')
end

-- main thread
thread_name, request_queue, response_queue = 'MAIN', puss.queue_create(), puss.queue_create()

local modules = puss._modules
local threads = {}
local THREAD_NUM = 1
for i=1, THREAD_NUM do
	threads[i] = puss.thread_create('puss.trace_dofile', puss._path..'/core/thread.lua', nil, 'PussThread:'..i, request_queue, response_queue)
end

local function check_thread_args(resp_module, resp_handle, pt)
	if resp_module then
		local module = modules[resp_module]
		assert( module, 'not find module:'..tostring(resp_module) )
		if resp_handle then
			assert( module[resp_handle], 'not find handle:'..tostring(resp_handle) )
		end
	end
	assert( pt )
end

__exports.broadcast = function(resp_module, resp_handle, pt, ...)
	check_thread_args(resp_module, resp_handle, pt)
	for i,thread in ipairs(threads) do
		thread:post(REPLY_QUERY, resp_module, resp_handle, pt, ...)
	end
end

__exports.query = function(resp_module, resp_handle, pt, ...)
	check_thread_args(resp_module, resp_handle, pt)
	return request_queue:push(REPLY_QUERY, resp_module, resp_handle, pt, ...)
end

__exports.pcall = function(timeout, pt, ...)
	assert( pt )
	local co, sk = puss.async_task_alarm(timeout or 60000)
	if not request_queue:push(REPLY_ASYNC, co, sk, pt, ...) then
		return puss.async_task_alarm()	-- cancel
	end
	return puss.async_task_yield()
end

replys[REPLY_QUERY] = function(module_name, handle_name, ...)
	if module_name then
		local module = modules[module_name]
		puss.trace_pcall(module[handle_name], ...)
	else
		for name, module in pairs(modules) do
			puss.trace_pcall(module[handle_name], ...)
		end
	end
end

replys[REPLY_ASYNC] = puss.async_service_resume

local function dispatch(reply_mode, k1, k2, ...)
	if not reply_mode then return true end
	replys[reply_mode](k1, k2, ...)
end

__exports.update = function()
	local ok, empty
	for i=1, 64 do
		ok, empty = puss.trace_pcall(dispatch, response_queue:pop(0))
		if ok and empty then break end
	end
end
