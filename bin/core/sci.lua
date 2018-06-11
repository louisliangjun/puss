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

local function do_reset_styles(sv, lang)
	local keywords, styles = keywords_map[lang], styles_map[lang]
	if keywords then
		sv:SetLexerLanguage(lang)
		sv:SetKeyWords(0, keywords)
	end
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
end

__exports.reset_styles = function(sv, lang)
	sv(false, do_reset_styles, lang)
end

__exports.create = function(lang)
	local sv = imgui.CreateScintilla()
	sv:SetTabWidth(4)
	sv(false, do_reset_styles, lang)
	return sv
end

