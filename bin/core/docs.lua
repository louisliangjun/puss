-- docs.lua

local app = puss.import('core.app')
local sci = puss.import('core.sci')

local function do_draw(page)
	-- local w, h = imgui.GetWindowContentRegionMax()
	imgui.BeginChild('Output', -1, -1, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		imgui.ScintillaUpdate(page.sv)
	imgui.EndChild()
end

__draw = do_draw

local function draw(page)
	return __draw(page)
end

local function new_doc(id, fname)
	page = app.create_page(id, fname, draw)
	page.sv = sci.create('lua')
	return page
end

__exports.new_page = function()
	return new_doc(nil, 'noname')
end

__exports.open = function(fname)
	local page = app.lookup_page(fname)
	if not page then
		local f = io.open(fname)
		if not f then return end
		local ctx = f:read('*a')
		f:close()

		page = new_doc(fname, fname)
		page.sv:SetText(ctx)
	end
	return page
end

