-- app.lua

-- local puss_editor = puss.import('editor')

__exports.open_file = function(label, filename, line)
	local f = io.open(filename, 'r')
	if not f then return end
	local cxt = f:read('*a')
	f:close()

	-- local ed = puss_editor.create(label, 'lua')
	-- ed:SetText(nil, cxt)
	-- ed:EmptyUndoBuffer()
	-- return ed
end

local function source_editor_main()
	imgui.Begin("Puss Editor")
	if imgui.Button("111") then
	end
	imgui.SameLine()
	if imgui.Button("222") then
	end
	imgui.SameLine()
	if imgui.Button("333") then
	end
	imgui.End()
end

__exports.main = function()
	local w = imgui.ImGuiCreateGLFW("imgui lua api", 1024, 768)
	while imgui.ImGuiUpdate(w, source_editor_main) do
		imgui.ImGuiRender(w)
	end
	imgui.ImGuiDestroy(w)
end

