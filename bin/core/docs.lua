-- docs.lua

local pages = puss.import('core.pages')
local sci = puss.import('core.sci')
local shotcuts = puss.import('core.shotcuts')
local fs = _fs

_inbuf = _inbuf or imgui.CreateByteArray(4*1024, 'find text')
_rebuf = _rebuf or imgui.CreateByteArray(4*1024, 'TODO : replace text')
local inbuf = _inbuf
local rebuf = _rebuf

local function do_save_page(page)
	page.unsaved = page.sv:GetModify()
	if not page.unsaved then
		return
	end
	if not page.filepath then
		page.saving = true
		return
	end
	local len, ctx = page.sv:GetText(page.sv:GetTextLength())
	if not fs.save(page.filepath, ctx) then
		page.saving = true
		page.saving_tips = 'save file failed, maybe readonly.'
		return
	end
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

local function page_call(page, cb, ...)
	imgui.BeginChild(page.label)
		page.sv(false, cb, ...)
	imgui.EndChild()
end

local function finish_show_dialog(page, active)
	if imgui.IsWindowFocused() and imgui.IsKeyPressed(PUSS_IMGUI_KEY_ESCAPE) then
		page.dialog_mode = nil
		active = true
	end
	if active then
		imgui.SetKeyboardFocusHere(-1)
	end
end

local function show_dialog_jump(page, active)
	if active then
		local line = page.sv:LineFromPosition(page.sv:GetCurrentPos())
		inbuf:strcpy(tostring(line+1))
	end

	if imgui.InputText('##JumpText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CharsDecimal) then
		local line = tonumber(inbuf:str())
		if line then
			active = false
			page.dialog_mode = nil	-- hide dialog
			page.scroll_to_line = line
		else
			active = true
		end
	end

	finish_show_dialog(page, active)
end

local function do_search(sv, text, length)
	local search_flags = 0
	sv:SetSearchFlags(search_flags)

	local ps = sv:GetSelectionStart()
	local pe = sv:GetSelectionEnd()
	if imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT) then
		sv:SetTargetStart(ps)
		sv:SetTargetEnd(0)
		ps = sv:SearchInTarget(length, text)
		if ps < 0 then
			sv:SetTargetStart(sv:GetTextLength())
			sv:SetTargetEnd(pe)
			ps = sv:SearchInTarget(length, text)
		end
	else
		sv:SetTargetStart(pe)
		sv:SetTargetEnd(sv:GetTextLength())
		pe = sv:SearchInTarget(length, text)
		if pe < 0 then
			sv:SetTargetStart(0)
			sv:SetTargetEnd(ps)
			ps = sv:SearchInTarget(length, text)
		else
			ps = pe
		end
	end
	if ps < 0 then
		sv:ClearSelections()
	else
		pe = ps + #text
		sv:SetSelection(ps, pe)
		sv:ScrollRange(ps, pe)
	end
end

local function show_dialog_find(page, active)
	if active then
		page_call(page, function(sv)
			local len, txt = sv:GetSelText()
			if len > 0 then inbuf:strcpy(txt) end
		end)
	end

	if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
		local text = inbuf:str()
		local length = #text
		active = true
		if length==0 then return end
		page_call(page, do_search, text, length)
	end

	finish_show_dialog(page, active)
end

local function show_dialog_replace(page, active)
	show_dialog_find(page, active)

	if imgui.InputText('##ReplaceText', rebuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
		print(mode, rebuf:str())
	end
end

local dialog_modes =
	{ {'jump', 'docs/jump', 1, show_dialog_jump}
	, {'find', 'docs/find', 1, show_dialog_find}
	, {'replace', 'docs/replace', 2, show_dialog_replace}
	}

function tabs_page_draw(page, active_page)
	if (not page.saving) and shotcuts.is_pressed('docs/save') then do_save_page(page) end
	if shotcuts.is_pressed('docs/close') then page.open = false end
	if page.saving then draw_saving_bar(page) end

	local active
	if imgui.IsWindowFocused(ImGuiFocusedFlags_ChildWindows) then
		for i,v in ipairs(dialog_modes) do
			if shotcuts.is_pressed(v[2]) then
				active = i
				break
			end
		end
	end

	local mode = active
	if active then
		page.dialog_mode = mode
	else
		mode = page.dialog_mode
	end
	mode = mode and dialog_modes[mode]

	if active_page then
		sci.reset_styles(page.sv, page.lang)
		imgui.SetNextWindowFocus()
	end

	if page.scroll_to_line then
		local line = page.scroll_to_line
		page.scroll_to_line = nil

		page_call(page, function(sv)
			sv:GotoLine(line-1)
			sv:ScrollCaret()
			imgui.SetWindowFocus()
		end)
	end

	local height = mode and -(mode[3] * imgui.GetFrameHeightWithSpacing()) or nil
	imgui.BeginChild(page.label, nil, height, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		page.sv(true)
		page.unsaved = page.sv:GetModify()
	imgui.EndChild()

	if mode then
		imgui.BeginGroup()
		imgui.Text(mode[1])
		imgui.SameLine()
		imgui.BeginGroup()
		mode[4](page, active)
		imgui.EndGroup()
		imgui.EndGroup()
	end
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

local generate_label = function(filename, filepath)
	return filename..'###'..fs.filename_hash(filepath)
end

local function new_doc(label, lang, filepath)
	local page = pages.create(label, _ENV)
	local sv = sci.create(lang)
	-- sv:SetViewWS(SCWS_VISIBLEALWAYS)
	page.lang = lang
	page.sv = sv
	page.filepath = filepath
	pages.active(label)
	return page
end

__exports.new_page = function()
	local label
	while true do
		last_index = (last_index or 0) + 1
		label = string.format('noname##%u', last_index)
		if not pages.lookup(label) then break end
	end
	return new_doc(label, 'lua')
end

__exports.open = function(filepath, line)
	filepath = puss.filename_format(filepath, true)
	local path, name = filepath:match('^(.*)/([^/]+)$')
	if not path then path, name = '', filepath end
	local label = generate_label(name, filepath)

	local page = pages.lookup(label)
	if page then
		pages.active(label)
	else
		local ctx = fs.load(filepath)
		if ctx then
			page = new_doc(label, sci.guess_language(name), filepath)
			page.sv:SetText(ctx)
			page.sv:EmptyUndoBuffer()
		end
	end
	if page and line and line>0 then
		page.scroll_to_line = line
	end
	return page
end

__exports.setup = function(new_fs)
	_fs = new_fs or fs
	fs = _fs
end

