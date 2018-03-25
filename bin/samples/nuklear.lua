-- nuklear.lua

function nuklear_demo_lua(ctx, sci)
	local LABEL = "LuaDemoWindow"
	-- nk_window_set_size(ctx, LABEL, nk_vec2(w, h))

	if nk_begin(ctx, LABEL, nk_rect(0, 0, 800, 600), NK_WINDOW_BACKGROUND) then
		nk_layout_row_static(ctx, 30, 80, 1);
		if nk_button_label(ctx, "button") then
			print('button pressed 1')
		end
		if nk_button_label(ctx, "button2") then
			print('button pressed 2')
		end
		if nk_button_label(ctx, "button3") then
			print('button pressed 3')
		end
    end
	nk_end(ctx)

	if nk_begin(ctx, "LuaDemo2", nk_rect(50, 50, 700, 500),
		NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
		NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)
    then
		nk_layout_row_static(ctx, 30, 80, 1);
		if nk_button_label(ctx, "button1") then
			print('LuaDemo2 button1 pressed')
		end
		if nk_button_label(ctx, "button2") then
			print('LuaDemo2 button2 pressed')
		end
		nk_scintilla_update(ctx, sci)
    end
	nk_end(ctx)
end

function __main__()
	local w = nk_glfw_window_create("nuklear lua api", 800, 600)
	local sci = nk_scintilla_new()
	sci:SetText("abcde中文fg")
	while w:update(nuklear_demo_lua, 0.001, sci) do
		w:draw()
	end
	w:destroy()
end

if not nk then
	local nk = puss.require('puss_nuklear_scintilla')
	_ENV.nk = nk
	setmetatable(_ENV, {__index=nk})
	puss.dofile(puss._script)	-- for use nk symbols & enums
end
