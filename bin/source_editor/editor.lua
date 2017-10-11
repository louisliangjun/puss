-- source_editor.lua

local app = puss.module_import('source_editor/app.lua')

function create(label)
	local ed = scintilla_new()
	ed:set_code_page(SC_CP_UTF8)
	ed:style_set_font(STYLE_DEFAULT, "monospace")
	local sw = gtk_scrolled_window_new()
	sw:add(ed)
	local doc_panel = app.main_builder:get_object('doc_panel')
	local label_widget = gtk_label_new()
	label_widget.label = label
	doc_panel:append_page(sw, label_widget)
	sw:show_all()
	return ed
end


function set_language(ed, lang)
	ed:set_lexer_language(nil, lang)
	if lang=='cpp' then
		local cpp_keywords = [[
			asm auto bool break case catch char class const 
			const_cast continue default delete do double 
			dynamic_cast else enum explicit extern false finally 
			float for friend goto if inline int long mutable 
			namespace new operator private protected public 
			register reinterpret_cast register return short signed 
			sizeof static static_cast struct switch template 
			this throw true try typedef typeid typename 
			union unsigned using virtual void volatile 
			wchar_t while
		]]

		ed:set_key_words(0, cpp_keywords)
		ed:style_set_fore(SCE_C_WORD, 0x00FF0000);
		ed:style_set_fore(SCE_C_STRING, 0x001515A3);
		ed:style_set_fore(SCE_C_CHARACTER, 0x00FF0000);
		ed:style_set_fore(SCE_C_PREPROCESSOR, 0x00808080);
		ed:style_set_fore(SCE_C_COMMENT, 0x00808080);
		ed:style_set_fore(SCE_C_COMMENTLINE, 0x00008000);
		ed:style_set_fore(SCE_C_COMMENTDOC, 0x00008000);
	end
end

