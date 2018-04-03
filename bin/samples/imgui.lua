-- nuklear.lua

local function imgui_demo_lua(imgui)
end

function __main__()
	local w = imgui_create("imgui lua api", 1024, 768)

	-- local sci = nk_scintilla_new()
	-- sci:SetText("abcde中文fg")
	while w:update(imgui_demo_lua, 0.001) do
		w:render()
	end
	w:destroy()
end

if not imgui then
	local imgui = puss.require('puss_imgui')
	_ENV.imgui = imgui
	setmetatable(_ENV, {__index=imgui})
	puss.dofile(puss._script)	-- for use nk symbols & enums
end
