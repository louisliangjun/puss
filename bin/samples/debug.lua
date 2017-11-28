function nuklear_demo_lua(ctx)
	local LABEL = "LuaDemoWindow"
	-- nk_window_set_size(ctx, LABEL, nk_vec2(w, h))

	if nk_begin(ctx, "DebugDemo", nk_rect(50, 50, 230, 250),
		NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
		NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)
    then
		nk_layout_row_static(ctx, 30, 80, 1);
		if nk_button_label(ctx, "demo") then
			print('DebugDemo button pressed')
			os.execute('./bin/puss tools/debugger.lua &')
		end
    end
	nk_end(ctx)
end

function __main__()
	puss.dofile('tools/debugger.lua')

	local w = nk_glfw_window_create("demo", 400, 300)
	while w do
		if w:update(nuklear_demo_lua) then
			w:destroy()
			w = nil
		else
			w:draw()
		end
	end
end

if not nk then
	local nk = puss.require('puss_nuklear')
	_ENV.nk = nk
	setmetatable(_ENV, {__index=nk})
	puss.dofile(puss._script)	-- for use nk symbols & enums
end

