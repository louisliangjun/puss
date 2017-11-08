puss.dofile('samples/utils.lua')

function lua_nuklear_demo(ctx)
	if nk_begin(ctx, "Lua Demo", nk_rect(50, 50, 230, 250),
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

