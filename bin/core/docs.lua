-- docs.lua

local app = puss.import('core.app')
local sci = puss.import('core.sci')
local shotcuts = puss.import('core.shotcuts')

_inbuf = _inbuf or imgui.CreateByteArray(4*1024, 'find text')
_rebuf = _rebuf or imgui.CreateByteArray(4*1024, 'replace text')
local inbuf = _inbuf
local rebuf = _rebuf

local function do_save_page(page)
	print(page.unsaved)
	page.unsaved = page.sv:GetModify()
	if not page.unsaved then
		return
	end
	if not page.filepath then
		page.saving = true
		return
	end
	local len, ctx = page.sv:GetText(page.sv:GetTextLength())
	local f = io.open(puss.utf8_to_local(page.filepath), 'w')
	if not f then
		page.saving = true
		page.saving_tips = 'save file failed, maybe readonly.'
		return
	end
	f:write(ctx)
	f:close()
	page.sv:SetSavePoint()
	page.saving = nil
	page.saving_tip = nil
	page.unsaved = nil
end

local function draw_saving_bar(page)
	local tips = page.saving_tips
	if tips then
		imgui.Text(tips)
	end

	if imgui.Button('cancel') then
		page.saving = nil
	end
	imgui.SameLine()
	if imgui.Button('save') then
		do_save_page(page)
	end
	imgui.SameLine()
	local pth = page.filepath
	if pth then
		imgui.Text(pth)
	else
		imgui.Text('TODO: input filename')
	end
	imgui.SameLine()
	if imgui.Button('close without save') then
		page.sv:SetSavePoint()
		page.saving = nil
		page.open = false
	end
end

local function draw_overlay_bar(page)
	local mode = page.show_overlay
	imgui.SetNextWindowBgAlpha(0.5)
	local show, open = imgui.Begin('##Layout', true, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav)
	if not show then return end

	imgui.Text(mode)
	if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_EnterReturnsTrue) then
		print(mode, inbuf:str())
	end
	--if mode=='jump' then end
	--if mode=='find' then end
	if mode=='replace' then
		if imgui.InputText('##ReplaceText', rebuf, ImGuiInputTextFlags_EnterReturnsTrue) then
			print(mode, rebuf:str())
		end
	end
	imgui.End()
end

function tabs_page_draw(page)
	if (not page.saving) and shotcuts.is_pressed('docs', 'save') then do_save_page(page) end
	if shotcuts.is_pressed('docs', 'close') then page.open = false end
	if page.saving then draw_saving_bar(page) end

	if shotcuts.is_pressed('docs', 'jump') then
		page.show_overlay = 'jump'
	elseif shotcuts.is_pressed('docs', 'find') then
		page.show_overlay = 'find'
	elseif shotcuts.is_pressed('docs', 'replace') then
		page.show_overlay = 'replace'
	end

	imgui.BeginChild('##SourceView', nil, nil, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		page.sv()
		page.unsaved = page.sv:GetModify()
	imgui.EndChild()

	if page.show_overlay then draw_overlay_bar(page) end
end

function tabs_page_save(page)
	do_save_page(page)
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

