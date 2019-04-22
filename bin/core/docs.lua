-- core.docs

local pages = puss.import('core.pages')
local sci = puss.import('core.sci')
local shotcuts = puss.import('core.shotcuts')
local hook = _hook or function(event, ...) end

_inbuf = _inbuf or imgui.CreateByteArray(4*1024)
_rebuf = _rebuf or imgui.CreateByteArray(4*1024)
local inbuf = _inbuf
local rebuf = _rebuf

shotcuts.register('docs/save', 'Save file', 'S', true, false, false, false)
shotcuts.register('docs/close', 'Close file', 'W', true, false, false, false)
shotcuts.register('docs/find', 'Find in file', 'F', true, false, false, false)
shotcuts.register('docs/jump', 'Jump in file', 'G', true, false, false, false)
shotcuts.register('docs/replace', 'Replace in file', 'H', true, false, false, false)
shotcuts.register('docs/quick_find', 'Quick find in file', 'F3', nil, nil, false, false)

local function do_save_page(page)
	page.unsaved = page.sv:GetModify()
	if not page.unsaved then
		return
	end
	if not page.filepath then
		page.saving = true
		return
	end

	local function page_after_save(ok, file_skey)
		if not ok then
			if page.file_skey~=file_skey then				
				page.saving_tips = 'file modified by other.'
			else
				page.saving_tips = 'save file failed, maybe readonly.'
			end
			return
		end
		page.sv:SetSavePoint()
		page.saving = nil
		page.saving_tip = nil
		page.unsaved = nil
		page.file_skey = file_skey
	end

	local len, ctx = page.sv:GetText(page.sv:GetTextLength()+1)
	page.saving = true
	puss.trace_pcall(hook, 'docs_page_on_save', page_after_save, page.filepath, ctx, page.file_skey)
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

local DOC_LABEL = '##SourceView'

local function page_call(page, cb, ...)
	imgui.BeginChild(DOC_LABEL)
		page.sv(cb, ...)
	imgui.EndChild()
end

local function do_search(sv, text, search_prev)
	local search_flags = 0
	sv:SetSearchFlags(search_flags)

	local length = #text
	local last_ps = sv:GetSelectionStart()
	local last_pe = sv:GetSelectionEnd()
	local ps, pe = last_ps, last_pe
	if search_prev then
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
		ps, pe = last_ps, last_pe
	else
		pe = ps + #text
	end
	sv:SetSelection(ps, pe)
	sv:ScrollRange(ps, pe)
end

local DIALOG_LABEL = '##DocPopup'
local dialog_modes = {}
local dialog_active = false
local dialog_focused = false
local dialog_show
local view_set_focus
local view_reset_style = true

local DOC_DRAW_MODE = _DOC_DRAW_MODE or 2
local DOC_SHOW_LINENUM = _DOC_SHOW_LINENUM ~= false
local DOC_SHOW_BP = _DOC_SHOW_BP ~= false
local DOC_FOLD_MODE = _DOC_FOLD_MODE or false
local DOC_WIN_FLAGS
local DOC_SCROLLBAR_SIZE

local edit_menu_clicked = nil

local function check_dialog_mode(page)
	local op = edit_menu_clicked
	edit_menu_clicked = nil

	for k,v in pairs(dialog_modes) do
		if op==k or shotcuts.is_pressed(k) then
			dialog_active, dialog_focused = true, false
			dialog_show = v
			imgui.OpenPopup(DIALOG_LABEL)
			break
		end
	end
end

local function show_dialog_begin(page, label)
	local x, y = imgui.GetWindowPos()
	local w = imgui.GetWindowWidth()
	imgui.SetNextWindowPos(x + w - DOC_SCROLLBAR_SIZE, y, ImGuiCond_Always, 1, 0)
	if not imgui.BeginPopup(DIALOG_LABEL) then
		dialog_show = nil
		return false
	end
	if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_ESCAPE) then imgui.CloseCurrentPopup() end
	imgui.Text(label)
	if not active then check_dialog_mode(page) end
	imgui.SameLine()
	return true
end

local function focus_last_input()
	if dialog_focused then return end
	if imgui.IsItemActive() then
		dialog_focused = true
	else
		imgui.SetKeyboardFocusHere(-1)
	end
end

local function active_find_text(page)
	page_call(page, function(sv)
		local len, text = sv:GetSelText()
		if len==0 then return end
		if text:byte(-1)==0 then text = text:sub(1,-2) end
		inbuf:strcpy(text)
		if page.find_text ~= text then
			page.find_text = text
			sci.find_text_fill_all_indicator(page.sv, text)
		end
	end)
end

dialog_modes['docs/jump'] = function(page, active)
	if active then
		local line = page.sv:LineFromPosition(page.sv:GetCurrentPos())
		inbuf:strcpy(tostring(line+1))
	end

	if show_dialog_begin(page, 'jump') then
		if imgui.InputText('##JumpText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CharsDecimal) then
			local line = tonumber(inbuf:str())
			if line then
				dialog_show = nil	-- hide dialog
				page.scroll_to_line, page.scroll_to_search_text = line-1, nil
				imgui.CloseCurrentPopup()
			end
		end
		focus_last_input()
		imgui.EndPopup()
	end
end

local find_input_text
do
	local FINDTEXT_FLAGS = ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackAlways
	local last_text

	local function find_input_text_on_edit(ev, page)
		local text = ev.Buf
		if last_text==text then return end
		last_text = text
		sci.find_text_fill_all_indicator(page.sv, text)
	end

	find_input_text = function(page)
		local search
		if imgui.InputText('##FindText', inbuf, FINDTEXT_FLAGS, find_input_text_on_edit, page) then
			local text = inbuf:str()
			if #text > 0 then
				if page.find_text ~= text then
					page.find_text = text
					sci.find_text_fill_all_indicator(page.sv, text)
				end
				search = (imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT)) and 1 or 2
			end
			imgui.SetKeyboardFocusHere(-1)
		end
		focus_last_input()

		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_UP) then search = 1 end
		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_DOWN) then search = 2 end
		return search
	end
end

dialog_modes['docs/find'] = function(page, active)
	if active then active_find_text(page) end

	local search
	if show_dialog_begin(page, 'find') then
		search = find_input_text(page)
		imgui.EndPopup()
	end

	if search then page_call(page, do_search, page.find_text, search==1) end
end

dialog_modes['docs/replace'] = function(page, active)
	if active then active_find_text(page) end

	local search
	if show_dialog_begin(page, 'find') then
		imgui.BeginGroup()
		search = find_input_text(page)

		imgui.InputText('##ReplaceText', rebuf)

		imgui.EndGroup()
		imgui.EndPopup()
	end

	if search then page_call(page, do_search, page.find_text, search==1) end
end

dialog_modes['docs/quick_find'] = function(page, active)
	if not active then return end

	if(imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_CONTROL) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_CONTROL)) then
		active_find_text(page)
	end

	page_call(page, function(sv)
		local text = inbuf:str()
		local search_prev = imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT)
		do_search(sv, text, search_prev)
	end)
end

function tabs_page_draw(page, active_page)
	if (not page.saving) and shotcuts.is_pressed('docs/save') then do_save_page(page) end
	if shotcuts.is_pressed('docs/close') then page.open = false end
	puss.trace_pcall(hook, 'docs_page_before_draw', page)
	if page.saving then draw_saving_bar(page) end

	if active_page then
		view_reset_style = true
		sci.reset_styles(page.sv, page.lang)
		page.sv:dirty_scroll()
		imgui.SetNextWindowFocus()
	end

	imgui.BeginChild(DOC_LABEL, nil, nil, false, DOC_WIN_FLAGS)
	local sv = page.sv
	if view_set_focus then
		view_set_focus = false
		imgui.SetWindowFocus()
	end

	local draw_mode = DOC_FOLD_MODE and 1 or DOC_DRAW_MODE

	if view_reset_style then
		view_reset_style = false
		_DOC_DRAW_MODE = DOC_DRAW_MODE
		_DOC_SHOW_LINENUM = DOC_SHOW_LINENUM
		_DOC_SHOW_BP = DOC_SHOW_BP
		_DOC_FOLD_MODE = DOC_FOLD_MODE

		DOC_WIN_FLAGS = (draw_mode==1) and ImGuiWindowFlags_AlwaysHorizontalScrollbar or (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		DOC_SCROLLBAR_SIZE = (draw_mode==1) and imgui.GetStyleVar(ImGuiStyleVar_ScrollbarSize) or 72

		sci.reset_show_linenum(sv, DOC_SHOW_LINENUM)
		sci.reset_show_bp(sv, DOC_SHOW_BP)
		sci.reset_fold_mode(sv, DOC_FOLD_MODE)
	end

	local scroll_to_line, search_text = page.scroll_to_line, page.scroll_to_search_text
	if scroll_to_line then
		page.scroll_to_line, page.scroll_to_search_text = nil, nil
		sv:GotoLine(scroll_to_line)
		sv:ScrollCaret()
		sv(draw_mode)
		if search_text then
			do_search(sv, search_text)
		else
			sv:GotoLine(scroll_to_line)
			sv:ScrollCaret()
		end
	else
		sv(draw_mode)
	end
	page.unsaved = sv:GetModify()
	puss.trace_pcall(hook, 'docs_page_after_draw', page)

	-- if imgui.IsWindowFocused(ImGuiFocusedFlags_ChildWindows) then check_dialog_mode(page) end
	check_dialog_mode(page)

	if dialog_show then
		local active = dialog_active
		dialog_active = false
		dialog_show(page, active)
	end
	imgui.EndChild()
end

function tabs_page_save(page)
	do_save_page(page)
end

function tabs_page_close(page)
	if page.unsaved then
		page.saving_tips = 'file not saved, need save ?'
		page.saving = true
		page.open = true
	end
end

function tabs_page_destroy(page)
	page.sv:destroy()
end

local function on_margin_click(sv, modifiers, pos, margin)
	local line = sv:LineFromPosition(pos)
	print(sv, modifiers, pos, margin, line)
	puss.trace_pcall(hook, 'docs_page_on_margin_click', sv:get('page'), modifiers, pos, margin, line)
end

local function new_doc(label, lang, filepath)
	local page = pages.create(label, _ENV)
	local sv = sci.create(lang)
	-- sv:SetViewWS(SCWS_VISIBLEALWAYS)
	sv:set('page', page)
	sv:set(SCN_MARGINCLICK, on_margin_click)
	page.lang = lang
	page.sv = sv
	page.filepath = filepath
	pages.active(label)
	puss.trace_pcall(hook, 'docs_page_on_create', page)
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

local function lookup_page(filepath)
	filepath = puss.filename_format(filepath)
	local path, name = filepath:match('^(.*)[/\\]([^/\\]+)$')
	if not path then path, name = '', filepath end
	local label = name..'###'..filepath
	local page = pages.lookup(label)
	return filepath, name, label, page
end

__exports.lookup = function(file)
	local filepath, name, label, page = lookup_page(file)
	return page, label
end

__exports.open = function(file, line, search_text)
	if not file then
		view_set_focus = true
		return
	end

	local filepath, name, label, page = lookup_page(file)
	if page then
		if line then page.scroll_to_line, page.scroll_to_search_text = line, search_text end
		if label ~= pages.selected() then
			pages.active(label)
		else
			view_set_focus = true
		end
		return
	end

	local function page_after_load(ctx, file_skey)
		if not ctx then return end
		page = pages.lookup(filepath)
		if page then
			pages.active(label)
		else
			page = new_doc(label, sci.guess_language(name), filepath)
		end
		local sv = page.sv
		if ctx:find('\r\n') then
			sv:SetEOLMode(SC_EOL_CRLF)
		elseif ctx:find('\n') then
			sv:SetEOLMode(SC_EOL_LF)
		end
		sv:SetText(ctx)
		-- sv:ConvertEOLs(sv:GetEOLMode())
		sv:EmptyUndoBuffer()
		page.file_skey = file_skey
		if line then page.scroll_to_line, page.scroll_to_search_text = line, search_text end
	end

	puss.trace_pcall(hook, 'docs_page_on_load', page_after_load, filepath)
end

local function on_margin_set(sv, filepath, line, value)
	print('on_set_bp', sv, filepath, line, value)
	if value then
		if sv:MarkerGet(line-1)==0 then
			sv:MarkerAdd(line-1, 0)
		end
	else
		sv:MarkerDelete(line-1, 0)
	end
end

__exports.margin_set = function(file, line, value)
	local filepath, name, label, page = lookup_page(file)
	if not page then return end
	page.sv(on_margin_set, filepath, line, value)
end

__exports.setup = function(new_hook)
	_hook = new_hook or hook
	hook = _hook
end

__exports.edit_menu_click = function(name)
	edit_menu_clicked = name
end

__exports.setting = function()
	local active, value
	active, value = imgui.Checkbox('LineNum', DOC_SHOW_LINENUM)
	if active then
		DOC_SHOW_LINENUM = value
		view_reset_style = true
	end
	active, value = imgui.Checkbox('Thumbnail Scrollbar', DOC_DRAW_MODE==2)
	if active then
		DOC_DRAW_MODE = value and 2 or 1
		view_reset_style = true
	end
	active, value = imgui.Checkbox('Fold Source Code', DOC_FOLD_MODE)
	if active then
		DOC_FOLD_MODE = value
		view_reset_style = true
	end
end
