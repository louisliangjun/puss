-- console.lua

local sci = puss.import('core.sci')

local function console_write(output, text)
	output:AppendText(#text, text)
end

local function console_execute(output, input)
	console_write(output, '> run:  ')
	console_write(output, input)
	console_write(output, '\n')

	local f, e = load(input, input, 't')
	if not f then
		console_write(output, '> load err: ')
		console_write(output, e)
		console_write(output, '\n')
		return
	end

	return f()
end

puss._print = puss._print or _G.print
local raw_print = puss._print

local function console_print(...)
	local output = _output
		raw_print('test')
	if output then
		local n = select('#', ...)
		for i=1, n do
			local v = select(i, ...)
			console_write(output, tostring(v))
			console_write(output, '\t')
		end
		console_write(output, '\n')
	end
	raw_print(...)
end

_G.print = console_print
__exports.log = console_print

__exports.update = function()
	_inbuf = _inbuf or imgui.ByteArrayCreate(4096)
	_output = _output or sci.create('lua')
	local inbuf, output = _inbuf, _output

	imgui.SetNextWindowSize(520, 600, ImGuiCond_FirstUseEver)
	imgui.Begin("Console", nil, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	local w, h = imgui.GetWindowContentRegionMax()
	imgui.BeginChild('Output', w-8, h-128, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		imgui.ScintillaUpdate(output)
	imgui.EndChild()
	if imgui.InputTextMultiline('', inbuf, w-8, 96, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput) then
		puss.trace_pcall(console_execute, output, inbuf:sub(1, #inbuf))
	end
	imgui.End()
end

