puss.dofile('samples/utils.lua')

function lua_nuklear_demo(ctx, w, h)
	local LABEL = "LuaDemoWindow"
	nk_window_set_size(ctx, LABEL, nk_vec2(w, h))

	if nk_begin(ctx, LABEL, nk_rect(0, 0, w, h), NK_WINDOW_BACKGROUND) then
		nk_layout_row_static(ctx, 30, 80, 1);
		if nk_button_label(ctx, "button") then
			print('button pressed')
		end
		if nk_button_label(ctx, "button2") then
			print('button pressed 2')
		end
		if nk_button_label(ctx, "button3") then
			print('button pressed 3')
		end
    end
	nk_end(ctx)

	if nk_begin(ctx, "LuaDemo2", nk_rect(50, 50, 230, 250),
		NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
		NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)
    then
		nk_layout_row_static(ctx, 30, 80, 1);
		if nk_button_label(ctx, "button") then
			print('button pressed')
		end
    end
	nk_end(ctx)
end

function __main__()
	print(application_run)

	application_run(nuklear_demo1)

	application_run(lua_nuklear_demo)
end

if not nk then
	local nk = puss.require('puss_nuklear')
	_ENV.nk = nk
	setmetatable(_ENV, {__index=nk})
	puss.dofile('samples/nuklear.lua')	-- for use nk symbols & enums
end

