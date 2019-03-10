-- core.sci

local language_settings = {}

local INDICATOR_FINDTEXT = 8

local DEFAULT_STYLE_DEFAULT = 0x00000000
local DEFAULT_STYLE_COMMENT = 0x00808080
local DEFAULT_STYLE_NUMBER = 0x07008000
local DEFAULT_STYLE_WORD = 0x00FF0000
local DEFAULT_STYLE_STRING = 0x001515A3
local DEFAULT_STYLE_PREPROCESSOR = 0x00808080
local DEFAULT_STYLE_OPERATOR = 0x7F007F00
local DEFAULT_STYLE_IDENTIFIER = 0x00000000

local default_setting =
	{ language = 'Text'
	, lexer = nil
	, keywords = nil
	, styles = nil
	, tab_width = 4
	, margin_linenum = true
	, margin_bp = true
	, sel_back = 0xe8bb9f
	, caret_line = 0xcfcfb0
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
		{ [SCE_LUA_DEFAULT] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_COMMENT] = DEFAULT_STYLE_COMMENT
		, [SCE_LUA_COMMENTLINE] = DEFAULT_STYLE_COMMENT
		, [SCE_LUA_COMMENTDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_LUA_NUMBER] = DEFAULT_STYLE_NUMBER
		, [SCE_LUA_WORD] = DEFAULT_STYLE_WORD
		, [SCE_LUA_STRING] = DEFAULT_STYLE_STRING
		, [SCE_LUA_CHARACTER] = DEFAULT_STYLE_STRING
		, [SCE_LUA_LITERALSTRING] = DEFAULT_STYLE_STRING
		, [SCE_LUA_PREPROCESSOR] = DEFAULT_STYLE_PREPROCESSOR
		, [SCE_LUA_OPERATOR] = DEFAULT_STYLE_OPERATOR
		, [SCE_LUA_IDENTIFIER] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_LUA_STRINGEOL] = DEFAULT_STYLE_STRING
		, [SCE_LUA_WORD2] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD3] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD4] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD5] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD6] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD7] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_WORD8] = DEFAULT_STYLE_DEFAULT
		, [SCE_LUA_LABEL] = 0x0000FF00
		}
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
		{ [SCE_C_DEFAULT] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_COMMENT] = DEFAULT_STYLE_COMMENT
		, [SCE_C_COMMENTLINE] = DEFAULT_STYLE_COMMENT
		, [SCE_C_COMMENTDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_C_NUMBER] = DEFAULT_STYLE_NUMBER
		, [SCE_C_WORD] = DEFAULT_STYLE_WORD
		, [SCE_C_STRING] = DEFAULT_STYLE_STRING
		, [SCE_C_CHARACTER] = DEFAULT_STYLE_STRING
		, [SCE_C_UUID] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_C_PREPROCESSOR] = DEFAULT_STYLE_PREPROCESSOR
		, [SCE_C_OPERATOR] = DEFAULT_STYLE_OPERATOR
		, [SCE_C_IDENTIFIER] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_C_STRINGEOL] = DEFAULT_STYLE_STRING
		, [SCE_C_VERBATIM] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_REGEX] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_COMMENTLINEDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_C_WORD2] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_COMMENTDOCKEYWORD] = DEFAULT_STYLE_COMMENT
		, [SCE_C_COMMENTDOCKEYWORDERROR] = DEFAULT_STYLE_COMMENT
		, [SCE_C_GLOBALCLASS] = 0x00008000
		, [SCE_C_STRINGRAW] = DEFAULT_STYLE_STRING
		, [SCE_C_TRIPLEVERBATIM] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_HASHQUOTEDSTRING] = DEFAULT_STYLE_STRING
		, [SCE_C_PREPROCESSORCOMMENT] = DEFAULT_STYLE_COMMENT
		, [SCE_C_PREPROCESSORCOMMENTDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_C_USERLITERAL] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_TASKMARKER] = DEFAULT_STYLE_DEFAULT
		, [SCE_C_ESCAPESEQUENCE] = DEFAULT_STYLE_DEFAULT
		}
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
		{ [SCE_SQL_DEFAULT] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_COMMENT] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_COMMENTLINE] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_COMMENTDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_NUMBER] = DEFAULT_STYLE_NUMBER
		, [SCE_SQL_WORD] = DEFAULT_STYLE_WORD
		, [SCE_SQL_STRING] = DEFAULT_STYLE_STRING
		, [SCE_SQL_CHARACTER] = DEFAULT_STYLE_STRING
		, [SCE_SQL_SQLPLUS] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_SQL_SQLPLUS_PROMPT] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_SQL_OPERATOR] = DEFAULT_STYLE_OPERATOR
		, [SCE_SQL_IDENTIFIER] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_SQL_SQLPLUS_COMMENT] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_COMMENTLINEDOC] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_WORD2] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_COMMENTDOCKEYWORD] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_COMMENTDOCKEYWORDERROR] = DEFAULT_STYLE_COMMENT
		, [SCE_SQL_USER1] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_USER2] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_USER3] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_USER4] = DEFAULT_STYLE_DEFAULT
		, [SCE_SQL_QUOTEDIDENTIFIER] = DEFAULT_STYLE_IDENTIFIER
		, [SCE_SQL_QOPERATOR] = DEFAULT_STYLE_OPERATOR
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

local function do_reset_styles(sv, lang)
	local setting = language_settings[lang] or default_setting
	if setting.lexer then sv:SetLexer(setting.lexer) end
	if setting.keywords then sv:SetKeyWords(0, setting.keywords) end

	-- color need clear all with STYLE_DEFAULT
	--sv:StyleSetBack(STYLE_DEFAULT, imgui.GetColorU32(ImGuiCol_FrameBg))
	--sv:StyleSetFore(STYLE_DEFAULT, imgui.GetColorU32(ImGuiCol_Text))
	sv:StyleSetBack(STYLE_DEFAULT, 0xFFF7FFFF)
	sv:StyleSetFore(STYLE_DEFAULT, 0x00000000)
	sv:StyleClearAll()
	if setting.styles then
		for k,v in pairs(setting.styles) do
			sv:StyleSetFore(k, v)
		end
	end

	sv:SetTabWidth(setting.tab_width)

	sv:SetSelBack(true, setting.sel_back)

	if setting.caret_line then
		sv:SetCaretLineVisible(true)
		sv:SetCaretLineBack(setting.caret_line)
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

	if setting.margin_linenum then
		sv:SetMarginTypeN(0, SC_MARGIN_NUMBER)
		sv:SetMarginWidthN(0, sv:TextWidth(STYLE_LINENUMBER, "_99999"))
		sv:SetMarginSensitiveN(0, true)
	else
		sv:SetMarginTypeN(0, SC_MARGIN_SYMBOL)
		sv:SetMarginWidthN(0, 0)
		sv:SetMarginSensitiveN(0, false)
	end

	if setting.margin_bp then
		sv:SetMarginTypeN(1, SC_MARGIN_SYMBOL)
		sv:SetMarginWidthN(1, 12)
		sv:SetMarginMaskN(1, 0x01)
		sv:SetMarginSensitiveN(1, true)

		sv:MarkerSetFore(0, 0x808000)
		sv:MarkerSetBack(0, 0x2000c0)
		sv:MarkerDefine(0, SC_MARK_ROUNDRECT)
	end

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

	local pos, rs, re = 0, 0, 0
	while true do
		pos, rs, re = sv:FindText(0, text, pos)
		if pos < 0 then break end
		sv:IndicatorFillRange(rs, re-rs)
		pos = pos + 1
	end
end

__exports.create = function(lang)
	local sv = imgui.CreateScintilla()
	sv(do_reset_styles, lang)
	return sv
end

