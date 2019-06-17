-- tccdbg.lua

local puss_tinycc = puss.load_plugin('puss_tinycc')

local event_handles = {}

event_handles.exception = function(first_chance, emsg, code, addr)
end

event_handles.debug_string = function(msg)
end

local function _default_event_handle(...)
end

local function on_event(ev, ...)
	print(ev, ...)
	puss.trace_pcall(event_handles[ev] or _default_event_handle, ...)
end

function __main__()
	local dbg = puss_tinycc.debug_attach(16260)
	print(dbg)
	while true do
		dbg:wait(on_event, 2000)
	end
	print('detach')
	dbg:detach()
	print('detached')
	puss.sleep(5000)
end

