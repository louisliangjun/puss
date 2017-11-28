-- debugger.lua

print('debugger', puss._script)

if puss._script~='tools/debugger.lua' then
	-- host debug server
	if not puss.debug then
		return print('ERROR : need run application with --debug')
	end

	local puss_socket = puss.require('puss_socket')

	local current_debugger
	local sock, addr = puss_socket.socket_udp_create(1*1024*1024, '127.0.0.1', 9999)
	print('host listen at:', addr)

	local function host_debug_update()
		local res, msg, from = sock:recvfrom()
		if not msg then return end
		current_debugger = from
		print( msg )
		if msg=='step_into' then
			puss.debug.step_into()
		elseif msg=='continue' then
			puss.debug.continue()
		end
	end

	local event_callbacks = {}

	event_callbacks[PUSS_DEBUG_EVENT_ATTACHED] = function()
		print('attached')
	end

	event_callbacks[PUSS_DEBUG_EVENT_UPDATE] = function()
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_HOOK_COUNT] = function()
		-- print('count')
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_BEGIN] = function()
		print('breaked begin')
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_UPDATE] = function()
		host_debug_update()
	end

	event_callbacks[PUSS_DEBUG_EVENT_BREAKED_END] = function()
		print('breaked end')
	end

	puss.debug.reset(function(ev) event_callbacks[ev]() end, 512)
else
	local sock

	function puss_debug_ui(ctx)
		local LABEL = "PussDebuggerWindow"
		-- nk_window_set_size(ctx, LABEL, nk_vec2(w, h))

		if nk_begin(ctx, LABEL, nk_rect(0, 0, 800, 600), NK_WINDOW_BACKGROUND) then
			nk_layout_row_static(ctx, 30, 80, 1);
			if nk_button_label(ctx, "connect") then
				sock:connect('127.0.0.1', 9999)
			end
			if nk_button_label(ctx, "step_into") then
				sock:send('step_into')
			end
			if nk_button_label(ctx, "continue") then
				sock:send('continue')
			end
		end
		nk_end(ctx)
	end

	function __main__()
		local puss_socket = puss.require('puss_socket')
		local w = nk_glfw_window_create("puss debugger", 800, 600)
		sock = puss_socket.socket_udp_create(1*1024*1024)

		while w do
			if w:update(puss_debug_ui) then
				w:destroy()
				w = nil
			else
				w:draw()
			end
		end
	end

	-- debugger
	if not nk then
		local nk = puss.require('puss_nuklear')
		_ENV.nk = nk
		setmetatable(_ENV, {__index=nk})
		puss.dofile(puss._script)	-- for use nk symbols & enums
	end
end

