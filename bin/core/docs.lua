-- docs.lua

local app = puss.import('core.app')
local sci = puss.import('core.sci')

function tabs_page_draw(page)
	imgui.BeginChild('Output', nil, nil, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		page.sv()
		page.unsaved = page.sv:GetModify()
	imgui.EndChild()
end

local function new_doc(fname)
	page = app.create_page(fname, _ENV)
	page.sv = sci.create('lua')
	return page
end

__exports.new_page = function()
	local label
	while true do
		last_index = (last_index or 0) + 1
		label = string.format('noname##%u', last_index)
		if not app.lookup_page(label) then break end
	end
	return new_doc(label)
end

__exports.open = function(fname)
	local page = app.lookup_page(fname)
	if not page then
		local f = io.open(fname)
		if not f then return end
		local ctx = f:read('*a')
		f:close()

		page = new_doc(fname)
		page.sv:SetText(ctx)
		page.sv:EmptyUndoBuffer()
	end
	return page
end

