-- docs.lua

local app = puss.import('core.app')
local sci = puss.import('core.sci')
local shotcuts = puss.import('core.shotcuts')

local function do_save_page(page)
	if not page.sv:GetModify() then return end
	if not page.filepath then
		page.saving = true
	else
		local len, ctx = page.sv:GetText(page.sv:GetTextLength())
		local f = io.open(puss.utf8_to_local(page.filepath), 'w')
		if f then
			f:write(ctx)
			f:close()
			page.sv:SetSavePoint()
		end
	end
end

function tabs_page_draw(page)
	if page.saving then
		if imgui.Button('save') then
			page.sv:SetSavePoint()
			page.saving = nil
			page.open = false
		end
		if imgui.Button('cancel') then
			page.saving = nil
		end
	elseif shotcuts.is_pressed('docs', 'save') then
		do_save_page(page)
	end
	imgui.BeginChild('Output', nil, nil, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		page.sv()
		page.unsaved = page.sv:GetModify()
	imgui.EndChild()
end

function tabs_page_close(page)
	if page.unsaved then
		page.saving = true
		page.open = true
	end
end

function tabs_page_destroy(page)
	page.sv:destroy()
end

local generate_label = function(filename, filepath) return filename..'###'..filepath end
if puss._sep=='\\' then
	generate_label = function(filename, filepath) return filename..'###'..filepath:lower() end
end

local function new_doc(label, lang, filepath)
	local page = app.create_page(label, _ENV)
	local sv = sci.create(lang)
	-- sv:SetViewWS(SCWS_VISIBLEALWAYS)
	page.sv = sv
	page.filepath = filepath
	return page
end

__exports.new_page = function()
	local label
	while true do
		last_index = (last_index or 0) + 1
		label = string.format('noname##%u', last_index)
		if not app.lookup_page(label) then break end
	end
	return new_doc(label, 'lua')
end

__exports.open = function(filepath)
	filepath = puss.filename_format(filepath)
	local path, name = filepath:match('^(.*)[/\\]([^/\\]+)$')
	if not path then path, name = '', filepath end
	local label = generate_label(name, filepath)

	local page = app.lookup_page(label)
	if page then
		app.active_page(label)
	else
		local f = io.open(puss.utf8_to_local(filepath))
		if not f then return end
		local ctx = f:read('*a')
		f:close()

		page = new_doc(label, sci.guess_language(name), filepath)
		page.sv:SetText(ctx)
		page.sv:EmptyUndoBuffer()
	end
	return page
end

