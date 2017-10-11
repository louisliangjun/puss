-- debug_console.lua
-- 

local glua = __glua__

local function debug_script_invoke(script)
	load(script, '<DEBUG-SCRIPT>')()
end

function puss_debug_panel_open()
	local tv = gtk_text_view_new()
	tv.buffer.text = '-- press ctrl+enter run script, do not forget add environ PUSS_DEBUG=1\nfor k,v in pairs(__glua__.types) do print(k,v) end\n'

	local function puss_debug_panel_output(s)
		local txt = tv.buffer.text
		txt = txt .. tostring(s) .. '\n'
		tv.buffer.text = txt
	end
	print = puss_debug_panel_output

	tv:signal_connect('key-release-event', function(view, ev)
		local st, kc = select(2, ev:get_state()), select(2, ev:get_keycode())
		if st==GDK_CONTROL_MASK and (kc==string.byte('\r') or kc==string.byte('\n')) then
			local text = view.buffer.text
			local ok,err = xpcall(debug_script_invoke, function(e) return debug.traceback(e,2) end, text)
			if not ok then puss_debug_panel_output('ERROR : ' .. err .. '\n\n') end
		end
	end)

	local sw = gtk_scrolled_window_new()
	sw:add(tv)
	local bottom_panel = main_builder:get_object('bottom_panel')
	local label = gtk_label_new()
	label.label = 'debug'
	bottom_panel:append_page(sw, label)
end

