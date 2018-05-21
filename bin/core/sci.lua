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

__exports.create = function(lang)
	local sv = imgui.ScintillaNew()
	sv:SetTabWidth(4)
	local keywords, styles = keywords_map[lang], styles_map[lang]
	if keywords then
		sv:SetLexerLanguage(lang)
		sv:SetKeyWords(0, keywords)
	end
	if styles then
		for k,v in pairs(styles) do
			sv:StyleSetFore(k, v)
		end
	end
	return sv
end

