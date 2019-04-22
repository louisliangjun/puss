-- core.sci

local language_settings = {}

local INDICATOR_FINDTEXT = 8
local STYLE_COLOR_MAP = {}

local function RGB(col)
	local r, g, b = col:match('^#(%x%x)(%x%x)(%x%x)$')
	return tonumber(r,16) | (tonumber(g,16)<<8) | (tonumber(b,16)<<16)
end

STYLE_COLOR_MAP.light =
	{ ['bg'] = RGB('#FFFFFC')
	, ['sel'] = RGB('#84B6DB')
	, ['caret_line'] = RGB('#A4D6FB')
	, ['marker_fore'] = RGB('#008080')
	, ['marker_back'] = RGB('#c00020')
	, ['text'] = RGB('#000000')
	, ['comment'] = RGB('#9F9F9F')
	, ['identifier'] = RGB('#000000')
	, ['number'] = RGB('#008000')
	, ['word'] = RGB('#0000FF')
	, ['string'] = RGB('#A31515')
	, ['preprocessor'] = RGB('#800080')
	, ['operator'] = RGB('#007F00')
	, ['label'] = RGB('#00FF00')
	}

STYLE_COLOR_MAP.dark = STYLE_COLOR_MAP.light

local function do_auto_indent_impl(sv, mode)
	sv:set(SCN_UPDATEUI, nil)
	local pos = sv:GetCurrentPos()
	local line = sv:LineFromPosition(pos)
	if line <= 0 then return end
	local indent = sv:GetLineIndentation(line-1)
	indent = indent + mode * (sv:GetTabIndents() and sv:GetTabWidth() or sv:GetIndent())
	if indent <= 0 then return end
	sv:SetLineIndentation(line, indent)
	sv:SetEmptySelection(sv:GetLineIndentPosition(line))
end

local function do_auto_indent(sv) do_auto_indent_impl(sv, 0) end
local function do_auto_indent_inc(sv) do_auto_indent_impl(sv, 1) end
local function do_auto_indent_dec(sv) do_auto_indent_impl(sv, -1) end

local default_setting =
	{ language = 'Text'
	, lexer = nil
	, keywords = nil
	, styles = nil
	, tab_width = 4
	, caret_line = true
	, margin_linenum = true
	, margin_bp = true
	, fold = false
	, on_char_added = nil
	}

local language_builders = {}

language_builders.lua = function(setting)
	setting.suffix = { 'lua', 'toc' }

	setting.keywords = [[
     and       break     do        else      elseif    end
     false     for       function  goto      if        in
     local     nil       not       or        repeat    return
     then      true      until     while
     self     _ENV
	]]

	setting.styles =
		{ [SCE_LUA_DEFAULT] = 'text'
		, [SCE_LUA_COMMENT] = 'comment'
		, [SCE_LUA_COMMENTLINE] = 'comment'
		, [SCE_LUA_COMMENTDOC] = 'comment'
		, [SCE_LUA_NUMBER] = 'number'
		, [SCE_LUA_WORD] = 'word'
		, [SCE_LUA_STRING] = 'string'
		, [SCE_LUA_CHARACTER] = 'string'
		, [SCE_LUA_LITERALSTRING] = 'string'
		, [SCE_LUA_PREPROCESSOR] = 'preprocessor'
		, [SCE_LUA_OPERATOR] = 'operator'
		, [SCE_LUA_IDENTIFIER] = 'identifier'
		, [SCE_LUA_STRINGEOL] = 'string'
		, [SCE_LUA_WORD2] = 'word'
		, [SCE_LUA_WORD3] = 'word'
		, [SCE_LUA_WORD4] = 'word'
		, [SCE_LUA_WORD5] = 'word'
		, [SCE_LUA_WORD6] = 'word'
		, [SCE_LUA_WORD7] = 'word'
		, [SCE_LUA_WORD8] = 'word'
		, [SCE_LUA_LABEL] = 'label'
		}

	local indent_map = { ['do']=1, ['else']=1, ['elseif']=1, ['then']=1, ['repeat']=1 }

	local function on_auto_indent(sv)
		local pos = sv:GetCurrentPos()
		local line = sv:LineFromPosition(pos)
		if line <= 0 then return end
		local ret, text = sv:GetLine(line-1)
		local last_word = text:match('.-(%w+)%s*$')
		local indent_mark = indent_map[last_word]
		if indent_mark then
			sv:set(SCN_UPDATEUI, (indent_mark>0) and do_auto_indent_inc or do_auto_indent_dec)
		else
			sv:set(SCN_UPDATEUI, do_auto_indent)
		end
	end

	setting.on_char_added = function(sv, ch)
		if ch==10 or ch==13 then return on_auto_indent(sv) end
	end
end

language_builders.cpp = function(setting)
	setting.suffix = { 'c', 'cc', 'cpp', 'cxx', 'h', 'hh', 'hpp', 'hxx', 'inl', 'inc' }

	setting.keywords = [[
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

	setting.styles =
		{ [SCE_C_DEFAULT] = 'text'
		, [SCE_C_COMMENT] = 'comment'
		, [SCE_C_COMMENTLINE] = 'comment'
		, [SCE_C_COMMENTDOC] = 'comment'
		, [SCE_C_NUMBER] = 'number'
		, [SCE_C_WORD] = 'word'
		, [SCE_C_STRING] = 'string'
		, [SCE_C_CHARACTER] = 'string'
		, [SCE_C_UUID] = 'identifier'
		, [SCE_C_PREPROCESSOR] = 'preprocessor'
		, [SCE_C_OPERATOR] = 'operator'
		, [SCE_C_IDENTIFIER] = 'identifier'
		, [SCE_C_STRINGEOL] = 'string'
		, [SCE_C_VERBATIM] = 'text'
		, [SCE_C_REGEX] = 'text'
		, [SCE_C_COMMENTLINEDOC] = 'comment'
		, [SCE_C_WORD2] = 'text'
		, [SCE_C_COMMENTDOCKEYWORD] = 'comment'
		, [SCE_C_COMMENTDOCKEYWORDERROR] = 'comment'
		, [SCE_C_GLOBALCLASS] = 'label'
		, [SCE_C_STRINGRAW] = 'string'
		, [SCE_C_TRIPLEVERBATIM] = 'text'
		, [SCE_C_HASHQUOTEDSTRING] = 'string'
		, [SCE_C_PREPROCESSORCOMMENT] = 'comment'
		, [SCE_C_PREPROCESSORCOMMENTDOC] = 'comment'
		, [SCE_C_USERLITERAL] = 'text'
		, [SCE_C_TASKMARKER] = 'text'
		, [SCE_C_ESCAPESEQUENCE] = 'text'
		}

	local indent_map =
		{ ['{']=1, [':']=1, ['else']=1
		}

	local function on_auto_indent(sv)
		local pos = sv:GetCurrentPos()
		local line = sv:LineFromPosition(pos)
		if line <= 0 then return end
		local ret, text = sv:GetLine(line-1)
		local last_word = text:match('([{:])%s*$') or text:match('.-(%w+)%s*$')
		local indent_mark = indent_map[last_word]
		if indent_mark then
			sv:set(SCN_UPDATEUI, (indent_mark>0) and do_auto_indent_inc or do_auto_indent_dec)
		else
			sv:set(SCN_UPDATEUI, do_auto_indent)
		end
	end

	setting.on_char_added = function(sv, ch)
		if ch==10 or ch==13 then return on_auto_indent(sv) end
	end
end

language_builders.sql = function(setting)
	setting.suffix = { 'sql' }

	setting.keywords = [[
		add all alter analyze and as asc asensitive before between bigint binary blob
		both by call cascade case change char character check collate column condition
		connection constraint continue convert create cross current_date current_time
		current_timestamp current_user cursor database databases day_hour
		day_microsecond day_minute day_second dec decimal declare default delayed
		delete desc describe deterministic distinct distinctrow div double drop dual
		each else elseif enclosed escaped exists exit explain false fetch float for
		force foreign from fulltext goto grant group having high_priority
		hour_microsecond hour_minute hour_second if ignore in index infile inner inout
		insensitive insert int integer interval into is iterate join key keys kill
		leading leave left like limit lines load localtime localtimestamp lock long
		longblob longtext loop low_priority match mediumblob mediumint mediumtext
		middleint minute_microsecond minute_second mod modifies natural not
		no_write_to_binlog null numeric on optimize option optionally or order out
		outer outfile precision primary procedure purge read reads real references
		regexp rename repeat replace require restrict return revoke right rlike schema
		schemas second_microsecond select sensitive separator set show smallint soname
		spatial specific sql sqlexception sqlstate sqlwarning sql_big_result
		sql_calc_found_rows sql_small_result ssl starting straight_join table
		terminated text then tinyblob tinyint tinytext to trailing trigger true undo
		union unique unlock unsigned update usage use using utc_date utc_time
		utc_timestamp values varbinary varchar varcharacter varying when where while
		with write xor year_month zerofill
	]]

	setting.styles =
		{ [SCE_SQL_DEFAULT] = 'text'
		, [SCE_SQL_COMMENT] = 'comment'
		, [SCE_SQL_COMMENTLINE] = 'comment'
		, [SCE_SQL_COMMENTDOC] = 'comment'
		, [SCE_SQL_NUMBER] = 'number'
		, [SCE_SQL_WORD] = 'word'
		, [SCE_SQL_STRING] = 'string'
		, [SCE_SQL_CHARACTER] = 'string'
		, [SCE_SQL_SQLPLUS] = 'identifier'
		, [SCE_SQL_SQLPLUS_PROMPT] = 'identifier'
		, [SCE_SQL_OPERATOR] = 'operator'
		, [SCE_SQL_IDENTIFIER] = 'identifier'
		, [SCE_SQL_SQLPLUS_COMMENT] = 'comment'
		, [SCE_SQL_COMMENTLINEDOC] = 'comment'
		, [SCE_SQL_WORD2] = 'text'
		, [SCE_SQL_COMMENTDOCKEYWORD] = 'comment'
		, [SCE_SQL_COMMENTDOCKEYWORDERROR] = 'comment'
		, [SCE_SQL_USER1] = 'text'
		, [SCE_SQL_USER2] = 'text'
		, [SCE_SQL_USER3] = 'text'
		, [SCE_SQL_USER4] = 'text'
		, [SCE_SQL_QUOTEDIDENTIFIER] = 'identifier'
		, [SCE_SQL_QOPERATOR] = 'operator'
		}
end

do
	local setting_mt = { __index=default_setting, __newindex=error }

	local function language_setting_create(lang, lexer,  builder)
		local setting = { lexer=lexer }
		language_settings[lang] = setting
		if builder then builder(setting) end
		setmetatable(setting, setting_mt)
		return setting
	end

	for language_name, lexer in pairs(imgui.GetScintillaLexers()) do
		local lang = language_name:lower()
		local builder = language_builders[lang]
		language_setting_create(lang, lexer, builder)
	end
end

do
	local lang_by_suffix = {}
	local lang_by_filename = {}

	local empty_suffix = {}
	for lang, setting in pairs(language_settings) do
		for _, suffix in ipairs(setting.suffix or empty_suffix) do
			suffix = suffix:lower()
			if lang_by_suffix[suffix] then
				print(string.format('WARNING: already exist suffix(%s) in language(%s)', suffix, lang))
			else
				lang_by_suffix[suffix] = lang
			end
		end
	end

	local lang_filename_map =
		{ ['lua'] = { 'vmake' }
		, ['makefile'] = { 'makefile' }
		}

	for lang, files in pairs(lang_filename_map) do
		for _, name in ipairs(files) do
			name = name:lower()
			if lang_by_filename[name] then
				print(string.format('WARNING: already exist filename(%s) in language(%s)', name, lang))
			else
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
end

_STYLE_VER = (_STYLE_VER or 0) + 1

local function fetch_sytel_col_map()
	local r,g,b = imgui.GetStyleColorVec4(ImGuiCol_Text)
	local dark = ((r+g+b)/3 > 0.5)
	return dark and STYLE_COLOR_MAP.dark or STYLE_COLOR_MAP.light
end

local function do_reset_styles(sv, lang)
	local setting = language_settings[lang] or default_setting
	if setting.lexer then sv:SetLexer(setting.lexer) end
	if setting.keywords then sv:SetKeyWords(0, setting.keywords) end
	local colmap = fetch_sytel_col_map()
	
	sv:StyleSetBack(STYLE_DEFAULT, colmap['bg'])
	sv:StyleSetFore(STYLE_DEFAULT, colmap['text'])
	sv:StyleSetSize(STYLE_DEFAULT, math.floor(imgui.GetFontSize()))
	sv:StyleClearAll()
	if setting.styles then
		for k,v in pairs(setting.styles) do
			sv:StyleSetFore(k, colmap[v])
		end
	end

	sv:SetTabWidth(setting.tab_width)

	sv:SetSelBack(true, colmap['sel'])

	if setting.caret_line then
		sv:SetCaretLineVisible(true)
		sv:SetCaretLineBack(colmap['caret_line'])
	else
		sv:SetCaretLineVisible(false)
	end
	sv:SetCaretLineVisibleAlways(true)

	sv:SetAdditionalSelectionTyping(true)
	sv:SetMouseSelectionRectangularSwitch(true)
	sv:SetVirtualSpaceOptions(SCVS_RECTANGULARSELECTION)

	sv:SetUseTabs(true)
	sv:SetTabIndents(true)
	sv:SetIndentationGuides(SC_IV_LOOKBOTH)

	-- linenum
	sv:SetMarginTypeN(0, SC_MARGIN_NUMBER)
	sv:SetMarginWidthN(0, 0)
	sv:SetMarginSensitiveN(0, true)

	-- bp color
	sv:SetMarginTypeN(1, SC_MARGIN_SYMBOL)
	sv:SetMarginMaskN(1, 0x01)
	sv:SetMarginWidthN(1, 0)
	sv:SetMarginSensitiveN(1, true)

	sv:MarkerSetFore(0, colmap['marker_fore'])
	sv:MarkerSetBack(0, colmap['marker_back'])
	sv:MarkerDefine(0, SC_MARK_ROUNDRECT)

	-- fold
	sv:SetProperty('fold', '1')
	sv:SetMarginTypeN(2, SC_MARGIN_SYMBOL)
	sv:SetMarginSensitiveN(2, true)
	sv:SetMarginMaskN(2, SC_MASK_FOLDERS)
	sv:SetMarginWidthN(2, 0)
	sv:SetAutomaticFold(SC_AUTOMATICFOLD_CLICK)

	sv:MarkerDefine(SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS) 
	sv:MarkerDefine(SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS)
	sv:MarkerDefine(SC_MARKNUM_FOLDEREND,  SC_MARK_CIRCLEPLUSCONNECTED)
	sv:MarkerDefine(SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED)
	sv:MarkerDefine(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE)
	sv:MarkerDefine(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE)
	sv:MarkerDefine(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE)
	sv:SetFoldFlags(16|4, 0)

	sv:set(SCN_CHARADDED, setting.on_char_added)

	sv:IndicSetStyle(INDICATOR_FINDTEXT, INDIC_FULLBOX)

	sv:set('sci.style', _STYLE_VER)
	sv:set('sci.lang', lang)
end

__exports.reset_styles = function(sv, lang)
	-- print('check reset_styles', sv:get('sci.style'), _STYLE_VER, sv:get('sci.lang'), lang)
	if sv:get('sci.style')==_STYLE_VER and sv:get('sci.lang')==lang then return end
	-- print('reset_styles', _STYLE_VER, lang)
	sv(do_reset_styles, lang)
end

__exports.find_text_fill_all_indicator = function(sv, text)
	sv:SetIndicatorCurrent(INDICATOR_FINDTEXT)
	sv:IndicatorClearRange(0, sv:GetLength())
	if not text or #text==0 then return end

	local pos, rs, re = 0, 0, 0
	while true do
		pos, rs, re = sv:FindText(0, text, pos)
		if pos < 0 then break end
		sv:IndicatorFillRange(rs, re-rs)
		pos = pos + 1
	end
end

__exports.reset_show_linenum = function(sv, show)
	sv:SetMarginWidthN(0, show and sv:TextWidth(STYLE_LINENUMBER, "_99999") or 0)
end

__exports.reset_show_bp = function(sv, show)
	sv:SetMarginWidthN(1, show and 14 or 0)
end

__exports.reset_fold_mode = function(sv, fold)
	sv:SetMarginWidthN(2, fold and 14 or 0)
end

__exports.create = function(lang)
	local sv = imgui.CreateScintilla()
	sv(do_reset_styles, lang)
	return sv
end

