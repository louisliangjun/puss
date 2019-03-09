-- docs.lua

local pages = puss.import('core.pages')
local sci = puss.import('core.sci')
local shotcuts = puss.import('core.shotcuts')
local hook = _hook or function(event, ...) end

_inbuf = _inbuf or imgui.CreateByteArray(4*1024, 'find text')
_rebuf = _rebuf or imgui.CreateByteArray(4*1024, 'TODO : replace text')
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

	local function page_after_save(ok)
		if not ok then
			page.saving_tips = 'save file failed, maybe readonly.'
			return
		end
		page.sv:SetSavePoint()
		page.saving = nil
		page.saving_tip = nil
		page.unsaved = nil
	end

	local len, ctx = page.sv:GetText(page.sv:GetTextLength()+1)
	page.saving = true
	puss.trace_pcall(hook, 'docs_page_on_save', page_after_save, page.filepath, ctx)
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
		page.sv(cb, ...)
	imgui.EndChild()
end

local FINDREPLACE_FLAGS = ( ImGuiWindowFlags_NoMove
	| ImGuiWindowFlags_NoDocking
	| ImGuiWindowFlags_NoTitleBar
	| ImGuiWindowFlags_NoResize
	| ImGuiWindowFlags_AlwaysAutoResize
	| ImGuiWindowFlags_NoSavedSettings
	| ImGuiWindowFlags_NoNav
	)

local function do_search(sv, text, search_prev)
	local search_flags = 0
	sv:SetSearchFlags(search_flags)

	local length = #text
	local ps = sv:GetSelectionStart()
	local pe = sv:GetSelectionEnd()
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
		sv:ClearSelections()
	else
		pe = ps + #text
		sv:SetSelection(ps, pe)
		sv:ScrollRange(ps, pe)
	end
end

local dialog_active = false
local dialog_modes = {}

local function check_dialog_mode(page)
	for k in pairs(dialog_modes) do
		if shotcuts.is_pressed(k) then
			dialog_active = true
			page.dialog_mode = k
			break
		end
	end
end

local function show_dialog_begin()
	local vid, x, y, w, h = imgui.GetWindowViewport()
	imgui.SetNextWindowPos(x + w - 320, y + 75, ImGuiCond_Always)
	return imgui.Begin('##findreplace', true, FINDREPLACE_FLAGS)
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

local function show_dialog_find(page, active)
	if active then active_find_text(page) end

	local search
	if show_dialog_begin() then
		imgui.Text('find')
		imgui.SameLine()
		if not active then check_dialog_mode(page) end
		if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
			local text = inbuf:str()
			active = true
			if #text==0 then return end
			if page.find_text ~= text then
				page.find_text = text
				sci.find_text_fill_all_indicator(page.sv, text)
			end
			search = (imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT)) and 1 or 2
		end
		imgui.SetItemDefaultFocus()

		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_UP) then search = 1 end
		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_DOWN) then search = 2 end

		if not imgui.IsWindowFocused() then
			active, page.dialog_mode = true, nil
		elseif imgui.IsKeyPressed(PUSS_IMGUI_KEY_ESCAPE) then
			active, page.dialog_mode = true, nil
		else
			imgui.SetKeyboardFocusHere()
		end
	end
	imgui.End()

	if search then page_call(page, do_search, page.find_text, search==1) end
end

dialog_modes['docs/jump'] = function(page)
	local active = dialog_active
	dialog_active = nil

	if active then
		local line = page.sv:LineFromPosition(page.sv:GetCurrentPos())
		inbuf:strcpy(tostring(line+1))
	end

	if show_dialog_begin() then
		imgui.Text('jump')
		imgui.SameLine()
		if active then
			imgui.SetWindowFocus()
			imgui.SetKeyboardFocusHere()
		else
			check_dialog_mode(page)
		end
		if imgui.InputText('##JumpText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CharsDecimal) then
			local line = tonumber(inbuf:str())
			if line then
				active, page.dialog_mode = false, nil	-- hide dialog
				page.scroll_to_line = line-1
			else
				active = true
			end
		end
		if imgui.IsWindowFocused() and imgui.IsKeyPressed(PUSS_IMGUI_KEY_ESCAPE) then
			active, page.dialog_mode = true, nil
		end
		if active then
			imgui.SetKeyboardFocusHere(-1)
		end
	end
	imgui.End()

end

dialog_modes['docs/find'] = function(page)
	local active = dialog_active
	dialog_active = nil
	show_dialog_find(page, active)
end

dialog_modes['docs/replace'] = function(page)
	local active = dialog_active
	dialog_active = nil
	if active then active_find_text(page) end

	local search
	if show_dialog_begin() then
		imgui.Text('find')
		imgui.SameLine()
		imgui.BeginGroup()
		if not active then check_dialog_mode(page) end
		if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
			local text = inbuf:str()
			active = true
			if #text==0 then return end
			if page.find_text ~= text then
				page.find_text = text
				sci.find_text_fill_all_indicator(page.sv, text)
			end
			search = (imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT)) and 1 or 2
		end
		imgui.SetItemDefaultFocus()

		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_UP) then search = 1 end
		if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_DOWN) then search = 2 end

		imgui.InputText('##ReplaceText', rebuf)

		imgui.EndGroup()

		if not imgui.IsWindowFocused() then
			active, page.dialog_mode = true, nil
		elseif imgui.IsKeyPressed(PUSS_IMGUI_KEY_ESCAPE) then
			active, page.dialog_mode = true, nil
		elseif active then
			imgui.SetKeyboardFocusHere()
		end
	end
	imgui.End()

	if search then page_call(page, do_search, page.find_text, search==1) end
end

dialog_modes['docs/quick_find'] = function(page)
	local active = dialog_active
	dialog_active = nil
	show_dialog_find(page, active and (imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_CONTROL) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_CONTROL)))

	if active then
		page_call(page, function(sv)
			local text = inbuf:str()
			local search_prev = imgui.IsKeyDown(PUSS_IMGUI_KEY_LEFT_SHIFT) or imgui.IsKeyDown(PUSS_IMGUI_KEY_RIGHT_SHIFT)
			do_search(sv, text, search_prev)
		end)
	end
end

function tabs_page_draw(page, active_page)
	if (not page.saving) and shotcuts.is_pressed('docs/save') then do_save_page(page) end
	if shotcuts.is_pressed('docs/close') then page.open = false end
	puss.trace_pcall(hook, 'docs_page_before_draw', page)
	if page.saving then draw_saving_bar(page) end

	if imgui.IsWindowFocused(ImGuiFocusedFlags_ChildWindows) then
		check_dialog_mode(page)
	end

	if active_page then
		sci.reset_styles(page.sv, page.lang)
		imgui.SetNextWindowFocus()
	end

	if page.scroll_to_line then
		local line = page.scroll_to_line
		page.scroll_to_line = nil

		page_call(page, function(sv)
			sv:GotoLine(line)
			sv:ScrollCaret()
			imgui.SetWindowFocus()
		end)
	end

	imgui.BeginChild(page.label, nil, nil, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		local sv = page.sv
		sv()
		page.unsaved = sv:GetModify()
		puss.trace_pcall(hook, 'docs_page_after_draw', page)
	imgui.EndChild()

	if page.dialog_mode then
		dialog_modes[page.dialog_mode](page)

		if not page.dialog_mode then
			page_call(page, function(sv) imgui.SetWindowFocus() end)
		end
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

local function on_margin_click(sv, modifiers, pos, margin)
	puss.trace_pcall(hook, 'docs_page_on_margin_click', sv:get('page'), modifiers, pos, margin)
end

local function on_auto_indent(sv, updated)
	sv:set(SCN_UPDATEUI, nil)
	local pos = sv:GetCurrentPos()
	local line = sv:LineFromPosition(pos)
	if line <= 0 then return end
	local ret, str = sv:GetLine(line-1)
	-- print('on_auto_indent', updated, pos, line, ret, str)
	str = str:match('^([ \t]+)')
	if str then sv:ReplaceSel(str) end
end

local function on_char_added(sv, ch)
	if ch==10 or ch==13 then
		sv:set(SCN_UPDATEUI, on_auto_indent)
	end
end

local function new_doc(label, lang, filepath)
	local page = pages.create(label, _ENV)
	local sv = sci.create(lang)
	-- sv:SetViewWS(SCWS_VISIBLEALWAYS)
	sv:set('page', page)
	sv:set(SCN_MARGINCLICK, on_margin_click)
	sv:set(SCN_CHARADDED, on_char_added)
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

__exports.open = function(file, line)
	local filepath, name, label, page = lookup_page(file)
	if page then
		if line then page.scroll_to_line = line end
		pages.active(label)
		return
	end

	local function page_after_load(ctx)
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
		if line then page.scroll_to_line = line end
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

