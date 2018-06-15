-- sci.lua

local keywords_map = {}
local styles_map = {}
do
	keywords_map.lua = [[
     and       break     do        else      elseif    end
     false     for       function  goto      if        in
     local     nil       not       or        repeat    return
     then      true      until     while
     self     _ENV
	]]

	styles_map.lua =
		{ [SCE_LUA_DEFAULT] = 0x00000000
		, [SCE_LUA_COMMENT] = 0x00808080
		, [SCE_LUA_COMMENTLINE] = 0x00008000
		, [SCE_LUA_COMMENTDOC] = 0x00008000
		, [SCE_LUA_NUMBER] = 0x07008000
		, [SCE_LUA_WORD] = 0x00FF0000
		, [SCE_LUA_STRING] = 0x001515A3
		, [SCE_LUA_CHARACTER] = 0x001515A3
		, [SCE_LUA_LITERALSTRING] = 0x001515A3
		, [SCE_LUA_PREPROCESSOR] = 0x00808080
		, [SCE_LUA_OPERATOR] = 0x7F007F00
		, [SCE_LUA_IDENTIFIER] = 0x00000000
		, [SCE_LUA_STRINGEOL] = 0x001515A3
		, [SCE_LUA_WORD2] = 0x00FF0000
		, [SCE_LUA_WORD3] = 0x00FF0000
		, [SCE_LUA_WORD4] = 0x00FF0000
		, [SCE_LUA_WORD5] = 0x00FF0000
		, [SCE_LUA_WORD6] = 0x00FF0000
		, [SCE_LUA_WORD7] = 0x00FF0000
		, [SCE_LUA_WORD8] = 0x00FF0000
		, [SCE_LUA_LABEL] = 0x0000FF00
		}

	keywords_map.cpp = [[
		void int char float double bool wchar_t
		struct union enum class typedef
		true false
		long short singed unsigned
		const volatile restrict
		auto register static extern thread_local mutable
		inline asm
		for while do
		break continue return goto
		if else switch case default
		new delete
		sizeof
		this friend virtual mutable explicit operator
		private protected public
		template typename
		namespace using
		throw try catch
		_Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local
	]]

	styles_map.cpp =
		{ [SCE_C_DEFAULT] = 0x00000000
		, [SCE_C_COMMENT] = 0x00808080
		, [SCE_C_COMMENTLINE] = 0x00808080
		, [SCE_C_COMMENTDOC] = 0x00008000
		, [SCE_C_NUMBER] = 0x07008000
		, [SCE_C_WORD] = 0x00FF0000
		, [SCE_C_STRING] = 0x001515A3
		, [SCE_C_CHARACTER] = 0x001515A3
		, [SCE_C_UUID] = 0x00001000
		, [SCE_C_PREPROCESSOR] = 0x00e08050
		, [SCE_C_OPERATOR] = 0x7F007F00
		, [SCE_C_IDENTIFIER] = 0x00000000
		, [SCE_C_STRINGEOL] = 0x001515A3
		, [SCE_C_VERBATIM] = 0x00000000
		, [SCE_C_REGEX] = 0x00000000
		, [SCE_C_COMMENTLINEDOC] = 0x00008000
		, [SCE_C_WORD2] = 0x00FF0000
		, [SCE_C_COMMENTDOCKEYWORD] = 0x00008000
		, [SCE_C_COMMENTDOCKEYWORDERROR] = 0x00008000
		, [SCE_C_GLOBALCLASS] = 0x00008000
		, [SCE_C_STRINGRAW] = 0x001515A3
		, [SCE_C_TRIPLEVERBATIM] = 0x00000000
		, [SCE_C_HASHQUOTEDSTRING] = 0x001515A3
		, [SCE_C_PREPROCESSORCOMMENT] = 0x00808080
		, [SCE_C_PREPROCESSORCOMMENTDOC] = 0x00808080
		, [SCE_C_USERLITERAL] = 0x00000000
		, [SCE_C_TASKMARKER] = 0x00000000
		, [SCE_C_ESCAPESEQUENCE] = 0x00000000
		}
end

local lang_by_suffix = {}
do
	local lang_suffix_map =
		{ ['ada'] = { 'ada' }
		, ['cpp'] = { 'c', 'cc', 'cpp', 'cxx', 'h', 'hh', 'hpp', 'hxx', 'inl', 'inc' }
		, ['lua'] = { 'lua' }
		, ['py'] = { 'python' }
		}
	for lang, files in pairs(lang_suffix_map) do
		for _, suffix in ipairs(files) do
			suffix = suffix:lower()
			if lang_by_suffix[suffix] then
				error(string.format('already exist suffix(%s) in language(%s)', suffix, lang))
			end
			lang_by_suffix[suffix] = lang
		end
	end
end

local lang_by_filename = {}
do
	local lang_filename_map =
		{ ['lua'] = { 'vmake' }
		, ['makefile'] = { 'makefile' }
		}
	for lang, files in pairs(lang_filename_map) do
		for _, name in ipairs(files) do
			name = name:lower()
			if lang_by_filename[name] then
				error(string.format('already exist filename(%s) in language(%s)', name, lang))
			end
			lang_by_filename[name] = lang
		end
	end
end

__exports.guess_language = function(filename)
	local lang = lang_by_filename[filename]
	if lang then return lang end

	local suffix = filename:match('^.*%.([^%.]+)$')
	lang = suffix and lang_by_suffix[suffix:lower()]
	if lang then return lang end
end

local function on_margin_click(sv, modifiers, pos, margin)
	local line = sv:LineFromPosition(pos)
	-- print(sv, modifiers, pos, margin, sv:MarkerGet(line))
	if (sv:MarkerGet(line) & 0x01)==0 then
		sv:MarkerAdd(line, 0)
	else
		sv:MarkerDelete(line, 0)
	end
end

_STYLE_VER = (_STYLE_VER or 0) + 1

local function do_reset_styles(sv, lang)
	local keywords, styles = keywords_map[lang], styles_map[lang]
	if keywords then
		sv:SetLexerLanguage(lang)
		sv:SetKeyWords(0, keywords)
	end

	-- color need clear all with STYLE_DEFAULT
	--sv:StyleSetBack(STYLE_DEFAULT, imgui.GetColorU32(ImGuiCol_FrameBg))
	--sv:StyleSetFore(STYLE_DEFAULT, imgui.GetColorU32(ImGuiCol_Text))
	sv:StyleSetBack(STYLE_DEFAULT, 0xFFF7FFFF)
	sv:StyleSetFore(STYLE_DEFAULT, 0x00000000)
	sv:StyleClearAll()
	if styles then
		for k,v in pairs(styles) do
			sv:StyleSetFore(k, v)
		end
	end

	sv:SetTabWidth(4)

	sv:SetSelBack(true, 0xe8bb9f)

	sv:SetCaretLineVisible(true)
	sv:SetCaretLineBack(0xb0ffff)

	sv:SetMarginTypeN(0, SC_MARGIN_NUMBER)
	sv:SetMarginWidthN(0, sv:TextWidth(STYLE_LINENUMBER, "_99999"))
	sv:SetMarginSensitiveN(0, true)

	sv:SetMarginTypeN(1, SC_MARGIN_SYMBOL)
	sv:SetMarginWidthN(1, 12)
	sv:SetMarginMaskN(1, 0x01)
	sv:SetMarginSensitiveN(1, true)

	sv:MarkerSetFore(0, 0x808000)
	sv:MarkerSetBack(0, 0x2000c0)
	sv:MarkerDefine(0, SC_MARK_ROUNDRECT)

	sv:set(SCN_MARGINCLICK, on_margin_click)

	sv:set('sci.style', _STYLE_VER)
	sv:set('sci.lang', lang)
end

__exports.reset_styles = function(sv, lang)
	-- print('check reset_styles', sv:get('sci.style'), _STYLE_VER, sv:get('sci.lang'), lang)
	if sv:get('sci.style')==_STYLE_VER and sv:get('sci.lang')==lang then return end
	-- print('reset_styles', _STYLE_VER, lang)
	sv(false, do_reset_styles, lang)
end

__exports.create = function(lang)
	local sv = imgui.CreateScintilla()
	sv(false, do_reset_styles, lang)
	return sv
end

