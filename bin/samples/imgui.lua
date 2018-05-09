-- nuklear.lua

local show_lua_window = true

local function imgui_demo_lua(sci)
	-- imgui.ShowUserGuide()
	imgui.ShowDemoWindow()
	if not show_lua_window then return end
	ok, show_lua_window = imgui.Begin("Lua Window", show_lua_window)
	imgui.Text("Hello from lua window!")
	if puss.debug and imgui.Button("start debugger") then
		puss.debug()
	end
	if imgui.Button("close") then
		show_lua_window = false
	end
	imgui.ScintillaUpdate(sci)
	imgui.End()
end

function __main__()
	local w = imgui.ImGuiCreateGLFW("imgui lua api", 1024, 768)
	local sci = imgui.ScintillaNew()
	do
		sci:SetTabWidth(4)
		sci:SetLexerLanguage('lua')
		local luaKeywords = [[
     and       break     do        else      elseif    end
     false     for       function  goto      if        in
     local     nil       not       or        repeat    return
     then      true      until     while
     self     _ENV
		]]

		sci:SetKeyWords(0, luaKeywords)

		sci:StyleSetFore(SCE_LUA_DEFAULT, 0x00000000)
		sci:StyleSetFore(SCE_LUA_COMMENT, 0x00808080)
		sci:StyleSetFore(SCE_LUA_COMMENTLINE, 0x00008000)
		sci:StyleSetFore(SCE_LUA_COMMENTDOC, 0x00008000)
		sci:StyleSetFore(SCE_LUA_NUMBER, 0x07008000)
		sci:StyleSetFore(SCE_LUA_WORD, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_STRING, 0x001515A3)
		sci:StyleSetFore(SCE_LUA_CHARACTER, 0x001515A3)
		sci:StyleSetFore(SCE_LUA_LITERALSTRING, 0x001515A3)
		sci:StyleSetFore(SCE_LUA_PREPROCESSOR, 0x00808080)
		sci:StyleSetFore(SCE_LUA_OPERATOR, 0x7F007F00)
		sci:StyleSetFore(SCE_LUA_IDENTIFIER, 0x00000000)
		sci:StyleSetFore(SCE_LUA_STRINGEOL, 0x001515A3)
		sci:StyleSetFore(SCE_LUA_WORD2, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD3, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD4, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD5, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD6, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD7, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_WORD8, 0x00FF0000)
		sci:StyleSetFore(SCE_LUA_LABEL, 0x0000FF00)
	end
	do
		local fname = puss._path .. '/samples/imgui.lua'
		local f = io.open(fname)
		local t = f:read('*a')
		f:close()
		sci:SetText(t)
		sci:EmptyUndoBuffer()
	end
	while ImGuiUpdate(w, imgui_demo_lua, sci) do
		ImGuiRender(w)
	end
	ImGuiDestroy(w)
end

if not imgui then
	_ENV.imgui = puss.require('puss_imgui')
	puss.dofile(puss._script)	-- for use nk symbols & enums
end
