-- console.lua

_output = _output or {}
_inbuf = _inbuf or imgui.ByteArrayCreate(4096)

local output = _output
local inbuf = _inbuf
local scroll_to_end = false

local function console_execute(input)
	table.insert(output, string.format('> run: %s', input))
	local f, e = load(input, input, 't')
	if f then return f() end
	table.insert(output, string.format('> load err: %s', e))
	scroll_to_end = true
end

puss._print = puss._print or _G.print
local raw_print = puss._print

local function console_print(...)
	local txt = {}
	local n = select('#', ...)
	for i=1, n do
		local v = select(i, ...)
		table.insert(txt, tostring(v))
	end
	table.insert(output, table.concat(txt, '\t'))
	scroll_to_end = true
	raw_print(...)
end

_G.print = console_print
__exports.log = console_print

local function console_update()
	imgui.BeginChild('##ConsoleOutput', 0, -128, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		imgui.PushStyleVar(ImGuiStyleVar_ItemSpacing, 4, 1)
		local r,g,b,a = imgui.GetStyleColorVec4(ImGuiCol_Text)
		for i,v in ipairs(output) do
			imgui.PushStyleColor(ImGuiCol_Text, 128,g,0,a)
			imgui.TextUnformatted(v)
			imgui.PopStyleColor()
		end
		if scroll_to_end then
			imgui.SetScrollHere(1)
			scroll_to_end = false
		end
		imgui.PopStyleVar()
	imgui.EndChild()

	imgui.Separator()

	local active, reclaim_focus = false, false
	if imgui.InputTextMultiline('##ConsoleInput', inbuf, -1, 0, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput) then
		reclaim_focus = true
		active = true
	elseif imgui.IsItemActive() then
		active = imgui.IsShortcutPressed(PUSS_IMGUI_KEY_KP_ENTER)
	end
	if active then puss.trace_pcall(console_execute, inbuf:str()) end
	if reclaim_focus then imgui.SetKeyboardFocusHere(-1) end
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(520, 600, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("Console", show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	if res then console_update() end
	imgui.End()
	return show
end

